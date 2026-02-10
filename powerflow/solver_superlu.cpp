// Minimal GridLAB-D external LU plugin using SuperLU (double precision)
// Exports: LU_init, LU_alloc, LU_solve, LU_destroy
// Build: see compile command below.

// SuperLU headers (double-precision driver)
extern "C" {
#include <slu_ddefs.h>   // main double-precision SuperLU interface
}

#include <cstdlib>
#include <cstring>
#include <vector>

// Include GridLAB-D NR solver structs so we can access a_LU/rows_LU/cols_LU/rhs_LU.
// Adjust the path if needed (your tree may differ).
// The struct fields used here (a_LU, rows_LU, cols_LU, rhs_LU) match typical GridLAB-D builds.
#include "powerflow.h"   // provides NR_SOLVER_VARS (or similar)

// ---- Internal plugin state ---------------------------------------------------
struct SUPERLU_STATE {
    int n = 0;        // matrix dimension (assumed square)
    int nnz = 0;      // number of nonzeros
    // CSC buffers reused across solves
    std::vector<int>    csc_colptr;   // length n+1
    std::vector<int>    csc_rowind;   // length nnz
    std::vector<double> csc_values;   // length nnz
};

// ---- External LU interface required by GridLAB-D ----------------------------

extern "C" {

// Allocate/initialize plugin state
void* LU_init(void* ext_array) {
    if (!ext_array) {
        SUPERLU_STATE* st = new SUPERLU_STATE();
        return static_cast<void*>(st);
    }
    return ext_array;
}

// Announce dimensions (and whether admittance changed) – can be used to resize buffers
void LU_alloc(void* ext_array, unsigned int rowcount, unsigned int colcount, bool /*admittance_change*/) {
    SUPERLU_STATE* st = static_cast<SUPERLU_STATE*>(ext_array);
    st->n   = static_cast<int>(rowcount);
    // nnz will be set in LU_solve once we see cols_LU[rowcount]
    st->csc_colptr.resize(st->n + 1);
    // rowind/values resized in LU_solve when nnz is known
}

// Convert CSR (row_ptr = cols_LU, col_ind = rows_LU, val = a_LU) to CSC and call SuperLU
int LU_solve(void* ext_array, NR_SOLVER_VARS* sys, unsigned int rowcount, unsigned int /*colcount*/) {
    SUPERLU_STATE* st = static_cast<SUPERLU_STATE*>(ext_array);
    const int n = static_cast<int>(rowcount);

    // Pointers from GridLAB-D (CSR)
    // NOTE: In many GridLAB-D builds, sys->cols_LU is row_ptr (length n+1),
    //       sys->rows_LU is col_ind (length nnz), sys->a_LU is values (length nnz).
    //       This convention is inferred from typical external-solver adapters.
    const int*    row_ptr = sys->cols_LU;
    const int*    col_ind = sys->rows_LU;
    const double* val     = sys->a_LU;

    if (!row_ptr || !col_ind || !val || !sys->rhs_LU) return -1;

    // nnz is last entry of row_ptr
    const int nnz = row_ptr[n];
    st->nnz = nnz;

    // Resize CSC buffers
    st->csc_rowind.assign(nnz, 0);
    st->csc_values.assign(nnz, 0.0);
    st->csc_colptr.assign(n + 1, 0);

    // --- CSR -> CSC conversion (O(nnz + n)) ---
    // 1) count entries per column
    for (int r = 0; r < n; ++r) {
        for (int k = row_ptr[r]; k < row_ptr[r + 1]; ++k) {
            int c = col_ind[k];                 // column index
            st->csc_colptr[c + 1]++;            // count one entry in column c
        }
    }
    // 2) exclusive prefix sum to get column pointers
    for (int c = 0; c < n; ++c) {
        st->csc_colptr[c + 1] += st->csc_colptr[c];
    }
    // 3) fill rowind/values using running column offsets
    std::vector<int> col_fill(st->csc_colptr.begin(), st->csc_colptr.end()); // copy of colptr to track fills
    for (int r = 0; r < n; ++r) {
        for (int k = row_ptr[r]; k < row_ptr[r + 1]; ++k) {
            int c = col_ind[k];
            int dst = col_fill[c]++;
            st->csc_rowind[dst] = r;
            st->csc_values[dst] = val[k];
        }
    }

    // --- Build SuperLU matrices (CompCol for A, Dense for RHS) ---
    SuperMatrix A, L, U, B;
    dCreate_CompCol_Matrix(&A, n, n, nnz,
                           st->csc_values.data(),       // nzval
                           st->csc_rowind.data(),       // rowind
                           st->csc_colptr.data(),       // colptr
                           SLU_NC, SLU_D, SLU_GE);      // column-compressed, double, general  // SuperLU API expects column-compressed form. citeturn15search36

    // rhs (will be overwritten by solution)
    dCreate_Dense_Matrix(&B, n, 1, sys->rhs_LU, n, SLU_DN, SLU_D, SLU_GE);

    // Permutations
    std::vector<int> perm_c(n, 0), perm_r(n, 0);

    // Options & statistics
    superlu_options_t options;
    set_default_options(&options);
    options.ColPerm = COLAMD;     // good default column ordering
    options.DiagPivotThresh = 1.0;// default threshold

    SuperLUStat_t stat;
    StatInit(&stat);

    int info = 0;
    dgssv(&options, &A, perm_c.data(), perm_r.data(), &L, &U, &B, &stat, &info);
    // dgssv factors A and overwrites B with the solution. citeturn15search36

    // Clean up SuperLU objects (but keep sys->rhs_LU which now holds the solution)
    Destroy_SuperNode_Matrix(&L);
    Destroy_CompCol_Matrix(&U);
    Destroy_SuperMatrix_Store(&A);
    Destroy_SuperMatrix_Store(&B);
    StatFree(&stat);

    // Return 0 on success; non-zero on failure
    return (info == 0) ? 0 : -1;
}

// Free plugin state
void LU_destroy(void* ext_array, bool /*new_iteration*/) {
    SUPERLU_STATE* st = static_cast<SUPERLU_STATE*>(ext_array);
    delete st;
}

} // extern "C"

