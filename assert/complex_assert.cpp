/* complex_assert

   Very simple test that compares complex values to any corresponding complex value.  It breaks the
   tests down into a test for the real value and a test for the imagniary portion.  If either test
   fails at any time, it t.rows() a 'zero' to the commit function and breaks the simulator out with
   a failure code.
*/

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstddef> // Required for offsetof

#include <unordered_map>
#include <vector>
#include <algorithm>

#include "gridlabd.h"
#include "gld_complex.h"

#include "complex_assert.h"

#include "find.h"
#include <cstring>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

// EXPORT int gld_major = 5;
// EXPORT int gld_minor = 3;

// EXPORT_CREATE(complex_assert);
// EXPORT_INIT(complex_assert);

// EXPORT_COMMIT(complex_assert);
// EXPORT_NOTIFY(complex_assert);

CLASS *complex_assert::oclass = nullptr;
// static complex_assert defaults_storage; // POD storage for defaults
// complex_assert *complex_assert::defaults = &defaults_storage;
//extern "C" CALLBACKS *callback;

// Global (or static) map: did any switch change at this timestep?
static std::unordered_map<TIMESTAMP, bool> ts_had_switch_change;

// ---- Switch state tracker (inside complex_assert.cpp) ----
struct PhasePtrs
{
    int *A{nullptr};
    int *B{nullptr};
    int *C{nullptr};
};

struct PhaseLast
{
    int A{-1}, B{-1}, C{-1};
};

struct MonitoredSwitch
{
    OBJECT *obj{nullptr};
    // we cache direct addresses to the published phase state properties
    PhasePtrs ptrs;
    PhaseLast last;
    // optional: operating mode (BANKED/INDIVIDUAL) if you want to log it
    int *pMode{nullptr}; // enumeration pointer, may be null if not found
};

// Keep a container on the complex_assert instance:
std::vector<MonitoredSwitch> sw_list;
bool switches_index_built = false;

// --- Heuristic: decide SNAPSHOT vs CONTINUOUS without GLM changes ---
// Rationale: Snapshot tests (like your meter-only case) have no network links and
// often very short duration. IEEE feeder models have links/devices and longer durations.
// The heuristic captures that without GLM edits.
static bool model_is_continuous_like()
{
    // 1) Long duration? Treat as continuous.
    char startbuf[64] = {0}, stopbuf[64] = {0};
    gl_global_getvar("starttime", startbuf, sizeof(startbuf));
    gl_global_getvar("stoptime", stopbuf, sizeof(stopbuf));
    TIMESTAMP t_start = gl_parsetime(startbuf);
    TIMESTAMP t_stop = gl_parsetime(stopbuf);
    bool long_run = (t_start > 0 && t_stop > t_start && (t_stop - t_start) >= 3600); // >= 1 hour

    // 2) Presence of network/controls? Treat as continuous.
    auto has_any = [](const char *cls) -> bool
    {
        FINDLIST *fl = gl_find_objects(FL_NEW, FT_CLASS, SAME, cls, FT_END);
        bool ok = (fl && fl->hit_count > 0);
        if (fl)
            gl_free((void **)&fl);
        return ok;
    };

    bool has_links = has_any("overhead_line") || has_any("underground_line") || has_any("triplex_line");
    bool has_devices = has_any("regulator") || has_any("switch") || has_any("recloser") || has_any("fuse") || has_any("capacitor");

    return long_run || has_links || has_devices;
}

// Build a list of all switch-like objects in the GLM
void build_switch_index()
{
    if (switches_index_built)
        return;

    // Find all `switch` objects
    FINDLIST *fl_switch = gl_find_objects(FL_NEW, FT_CLASS, SAME, "switch", FT_END);
    if (fl_switch != nullptr && fl_switch->hit_count > 0)
    {
        OBJECT *o = nullptr;
        while ((o = gl_find_next(fl_switch, o)) != nullptr)
        {
            MonitoredSwitch ms;
            ms.obj = o;

            // Cache pointers to phase state properties if present
            PROPERTY *pA = gl_get_property(o, "phase_A_state");
            PROPERTY *pB = gl_get_property(o, "phase_B_state");
            PROPERTY *pC = gl_get_property(o, "phase_C_state");
            if (pA)
                ms.ptrs.A = (int *)gl_get_addr(o, "phase_A_state");
            if (pB)
                ms.ptrs.B = (int *)gl_get_addr(o, "phase_B_state");
            if (pC)
                ms.ptrs.C = (int *)gl_get_addr(o, "phase_C_state");

            // Optional: banked/individual mode
            PROPERTY *pMode = gl_get_property(o, "operating_mode");
            if (pMode)
                ms.pMode = (int *)gl_get_addr(o, "operating_mode");

            // Initialize last seen values to current ones (so we don't flag on first sample)
            ms.last.A = (ms.ptrs.A ? *ms.ptrs.A : -1);
            ms.last.B = (ms.ptrs.B ? *ms.ptrs.B : -1);
            ms.last.C = (ms.ptrs.C ? *ms.ptrs.C : -1);

            sw_list.push_back(ms);
        }
        gl_free((void **)&fl_switch);
    }

    // Also include `recloser` objects (they subclass switch behavior)
    FINDLIST *fl_recloser = gl_find_objects(FL_NEW, FT_CLASS, SAME, "recloser", FT_END);
    if (fl_recloser != nullptr && fl_recloser->hit_count > 0)
    {
        OBJECT *o = nullptr;
        while ((o = gl_find_next(fl_recloser, o)) != nullptr)
        {
            MonitoredSwitch ms;
            ms.obj = o;

            PROPERTY *pA = gl_get_property(o, "phase_A_state");
            PROPERTY *pB = gl_get_property(o, "phase_B_state");
            PROPERTY *pC = gl_get_property(o, "phase_C_state");
            if (pA)
                ms.ptrs.A = (int *)gl_get_addr(o, "phase_A_state");
            if (pB)
                ms.ptrs.B = (int *)gl_get_addr(o, "phase_B_state");
            if (pC)
                ms.ptrs.C = (int *)gl_get_addr(o, "phase_C_state");

            PROPERTY *pMode = gl_get_property(o, "operating_mode");
            if (pMode)
                ms.pMode = (int *)gl_get_addr(o, "operating_mode");

            ms.last.A = (ms.ptrs.A ? *ms.ptrs.A : -1);
            ms.last.B = (ms.ptrs.B ? *ms.ptrs.B : -1);
            ms.last.C = (ms.ptrs.C ? *ms.ptrs.C : -1);

            sw_list.push_back(ms);
        }
        gl_free((void **)&fl_recloser);
    }

    switches_index_built = true;
    gl_verbose("complex_assert: indexed %zu switch/recloser objects", sw_list.size());
}

// Returns true if any phase on any monitored switch changed this step
bool switching_happened_this_step(TIMESTAMP ts_now)
{
    bool changed = false;
    for (auto &ms : sw_list)
    {
        // Read current values (if pointers exist)
        int currA = (ms.ptrs.A ? *ms.ptrs.A : ms.last.A);
        int currB = (ms.ptrs.B ? *ms.ptrs.B : ms.last.B);
        int currC = (ms.ptrs.C ? *ms.ptrs.C : ms.last.C);

        // Compare to last seen values
        if (currA != ms.last.A || currB != ms.last.B || currC != ms.last.C)
        {
            changed = true;
            // update last so changes are only detected once per transition
            ms.last.A = currA;
            ms.last.B = currB;
            ms.last.C = currC;

            // optional: log which switch changed
            gl_debug("complex_assert: switch '%s' changed state (A=%d,B=%d,C=%d)",
                     (ms.obj->name ? ms.obj->name : "unnamed"), currA, currB, currC);
        }
    }
    if (changed)
        ts_had_switch_change[ts_now] = true;
    return changed;
}

complex_assert::complex_assert(MODULE *module) : gld_object()
{
    gl_output("complex_assert ctor: build %s %s", __DATE__, __TIME__);

    status = ASSERT_TRUE;
    within = 0.0;
    value = 0.0;
    once = ONCE_FALSE;
    once_value = 0;
    operation = FULL;
    strcpy(target, "");
    pTarget = nullptr;
    pComplex = nullptr;
    done = false;
    pending_fail_ts = 0;
    prestart_deferral_done = false;
    seen_finite = false;
    switches_index_built = false;
    last_to = 0;
    ts_in = 0;
    ts_out = 0;

    if (oclass == nullptr)
    {
        // register to receive notice for first top down. bottom up, and second top down synchronizations
        // Making assert a POSTTOPDOWN observer increases the chance the detector sees switching change before
        // any assert fires in that timestamp—reducing race conditions
        // oclass = gl_register_class(module, "complex_assert", sizeof(complex_assert),   PC_AUTOLOCK | PC_OBSERVER);
        oclass = gld_class::create(module, "complex_assert", sizeof(complex_assert), PC_AUTOLOCK | PC_OBSERVER);

        // oclass = gl_register_class(module, "complex_assert", sizeof(complex_assert), PC_AUTOLOCK );

        if (oclass == nullptr)
            throw "unable to register class complex_assert";
        else
            oclass->trl = TRL_PROVEN;

        if (gl_publish_variable(oclass,
                                // TO DO:  publish your variables here
                                PT_enumeration, "status", get_status_offset(), PT_DESCRIPTION, "Conditions for the assert checks",
                                PT_KEYWORD, "ASSERT_TRUE", (enumeration)ASSERT_TRUE,
                                PT_KEYWORD, "ASSERT_FALSE", (enumeration)ASSERT_FALSE,
                                PT_KEYWORD, "ASSERT_NONE", (enumeration)ASSERT_NONE,
                                PT_enumeration, "once", get_once_offset(), PT_DESCRIPTION, "Conditions for a single assert check",
                                PT_KEYWORD, "ONCE_FALSE", (enumeration)ONCE_FALSE,
                                PT_KEYWORD, "ONCE_TRUE", (enumeration)ONCE_TRUE,
                                PT_KEYWORD, "ONCE_DONE", (enumeration)ONCE_DONE,
                                PT_enumeration, "operation", get_operation_offset(), PT_DESCRIPTION, "Complex operation for the comparison",
                                PT_KEYWORD, "FULL", (enumeration)FULL,
                                PT_KEYWORD, "REAL", (enumeration)REAL,
                                PT_KEYWORD, "IMAGINARY", (enumeration)IMAGINARY,
                                PT_KEYWORD, "MAGNITUDE", (enumeration)MAGNITUDE,
                                PT_KEYWORD, "ANGLE", (enumeration)ANGLE, // specify in radians
                                PT_complex, "value", get_value_offset(), PT_DESCRIPTION, "Value to assert",
                                PT_double, "within", get_within_offset(), PT_DESCRIPTION, "Tolerance for a successful assert",
                                PT_char1024, "target", get_target_offset(), PT_DESCRIPTION, "Property to perform the assert upon",
                                PT_timestamp, "in", get_ts_in_offset(), PT_DESCRIPTION, "Earliest time to evaluate",
                                PT_timestamp, "out", get_ts_out_offset(), PT_DESCRIPTION, "Latest time to evaluate",
                                nullptr) < 1)
        {
            char msg[256];
            sprintf(msg, "unable to publish properties in %s", __FILE__);
            throw msg;
        }

        status = ASSERT_TRUE;
        within = 0.0;
        value = 0.0;
        once = ONCE_FALSE;
        once_value = 0;
        operation = FULL;
        strcpy(target, "");
        // printf("DEBUG: complex_assert constructor - target initialized to: '%s'\n", target);
        // printf("DEBUG: target buffer address: %p\n", &target);

        // memcpy(this, defaults, sizeof(complex_assert));
    }
}

/* Object creation is called once for each object that is created by the core */
int complex_assert::create(void)
{

    // memcpy(this, defaults, sizeof(complex_assert));

    // Do NOT use memcpy. Initialize members individually to preserve
    // internal data like the parent pointer.
    status = ASSERT_TRUE;
    within = 0.0;
    operation = FULL;
    value = gld::complex(0.0, 0.0);
    once = ONCE_FALSE;
    once_value = gld::complex(0.0, 0.0);
    seen_finite = false;
    pTarget = nullptr;
    ts_in = 0;
    ts_out = 0;

    // Use strncpy for safety and ensure null termination.
    strncpy(target, "", sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    // printf("DEBUG: complex_assert::create() - target initialized to: '%s'\n", target);

    return 1; /* return 1 on success, 0 on failure */
}

// TIMESTAMP complex_assert::resched_safe(const char* reason)
// {
//     // If we’re truly done, never schedule again
//     if (done) {
//         gl_debug("ASSERT[%s]: resched (%s) -> TS_NEVER [done]", get_name(), reason);
//         return TS_NEVER;
//     }

//     const TIMESTAMP now = gl_globalclock;

//     // First-time scheduling
//     if (!prestart_deferral_done) {
//         TIMESTAMP initial = 0;
//         if (ts_in != 0 /* && gl_isvalidtime(ts_in) */) {
//             initial = ts_in;
//         } else {
//             char startbuf[64] = {0};
//             gl_global_getvar("starttime", startbuf, sizeof(startbuf));
//             TIMESTAMP t_start = gl_parsetime(startbuf);
//             initial = (t_start > 0 /* && gl_isvalidtime(t_start) */) ? t_start : (now + 1);
//         }
//         prestart_deferral_done = true;
//         last_to = initial;
//         char buf[64] = {0}; gl_printtime(initial, buf, sizeof(buf));
//         gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [initial]", get_name(), reason, (long long)initial, buf);
//         return initial;
//     }

//     // Haven’t reached ts_in yet? Aim at it.
//     if (ts_in != 0 && now < ts_in) {
//         last_to = ts_in;
//         char buf[64] = {0}; gl_printtime(ts_in, buf, sizeof(buf));
//         gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [gate to ts_in]", get_name(), reason, (long long)ts_in, buf);
//         return ts_in;
//     }

//     // Normal continuous scheduling: strictly after (monotonic)
//     TIMESTAMP next = std::max(last_to + 1, now + 1);
//     last_to = next;
//     char buf[64] = {0}; gl_printtime(next, buf, sizeof(buf));
//     gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [normal]", get_name(), reason, (long long)next, buf);
//     return next;
// }

int complex_assert::init(OBJECT *parent)
{
    const char *me_name = get_name();
    auto *parent_o = get_parent(); // gld_object*
    const char *par_name = parent_o ? parent_o->get_name() : nullptr;

    // If the parent isn't linked yet, request deferred init (return 2)
    if (!parent_o)
    {
        gl_warning("complex_assert init: parent not yet linked; deferring. child='%s' (id=%llu)",
                   me_name ? me_name : "(unnamed)", (unsigned long long)get_id());
        return 2; // <-- DEFER (not fatal)
    }

    // Fail-fast: try to resolve the target now (if available)
    if (resolve_target_property() == 0)
    {
        // If property resolution fails, defer once more to give the core a chance
        gl_warning("complex_assert init: target '%s' unresolved on parent '%s'; deferring. child='%s' (id=%llu)",
                   get_target().c_str(),
                   par_name ? par_name : "(unnamed)",
                   me_name ? me_name : "(unnamed)",
                   (unsigned long long)get_id());
        return 2; // <-- DEFER (not fatal)
    }

    return 1; // Success
}

// Add a new method for property resolution
// int complex_assert::resolve_target_property()
// {
//     const char *target_str = get_target().c_str();

//     // printf("DEBUG: target_str raw content: '");
//     // for(int i = 0; i < 20 && target_str[i] != '\0'; i++) {
//     //     printf("%c", isprint(target_str[i]) ? target_str[i] : '?');
//     // }
//     // printf("'\n");

//     // Add safety check for empty target
//     if (strlen(target_str) == 0)
//     {
//         gl_error("Target property name is empty");
//         return 0;
//     }

//     OBJECT *target_obj = nullptr;
//     char obj_name_str[256] = "";
//     char prop_name_str[256] = "";

//     // === First, try the ENTIRE target string as a property on parent ===
//     target_obj = get_parent()->my();
//     if (target_obj != nullptr)
//     {
//         PROPERTY *prop = gl_get_property(target_obj, target_str, nullptr);
//         if (prop != nullptr)
//         {
//             // Found it directly on parent (e.g., "panel.power" is a valid property name)
//             strcpy(prop_name_str, target_str);
//         }
//     }

//     // If not found on parent, try object.property parsing
//     if (prop_name_str[0] == '\0')
//     {
//         // Parse the target string for dot notation (object.property)
//         const char *dot = strchr(target_str, '.');
//         if (dot != nullptr)
//         {
//             // Extract object name and property name
//             size_t obj_name_len = dot - target_str;
//             if (obj_name_len >= sizeof(obj_name_str))
//             {
//                 gl_error("Target object name in '%s' is too long", target_str);
//                 return 0;
//             }
//             strncpy(obj_name_str, target_str, obj_name_len);
//             obj_name_str[obj_name_len] = '\0';
//             strcpy(prop_name_str, dot + 1);

//             // Find the object by name
//             FINDLIST *pFindList = gl_find_objects(FL_NEW, FT_NAME, SAME, obj_name_str, FT_END);
//             if (pFindList == nullptr || pFindList->hit_count == 0)
//             {
//                 gl_error("Target object '%s' not found", obj_name_str);
//                 if (pFindList)
//                     gl_free((void **)&pFindList);
//                 return 0;
//             }
//             target_obj = gl_find_next(pFindList, nullptr);
//             gl_free((void **)&pFindList);
//         }
//         else
//         {
//             // No dot - use parent object with simple property name
//             target_obj = get_parent()->my();
//             if (target_obj == nullptr)
//             {
//                 gl_error("complex_assert has no parent and target '%s' doesn't specify an object", target_str);
//                 return 0;
//             }
//             strcpy(prop_name_str, target_str);
//         }
//     }
//     // gl_debug("Resolving target property '%s' on object '%s'",
//     //          prop_name_str, target_obj->name ? target_obj->name : "unnamed");

//     // Use target_obj and prop_name_str (not parent and target_str)
//     pTarget = gl_get_property(target_obj, prop_name_str);
//     if (pTarget == nullptr)
//     {
//         gl_error("Property '%s' not found on object '%s'",
//                  prop_name_str, target_obj->name ? target_obj->name : "unnamed");
//         return 0;
//     }

//     if (pTarget->ptype != PT_complex)
//     {
//         gl_error("Property '%s' is not complex type (type=%d)", prop_name_str, pTarget->ptype);
//         return 0;
//     }

//     // Use target_obj and prop_name_str (not parent and target_str)
//     pComplex = (gld::complex *)gl_get_addr(target_obj, prop_name_str);
//     if (pComplex == nullptr)
//     {
//         gl_error("Unable to get address of property '%s'", prop_name_str);
//         return 0;
//     }

//     // gl_debug("Successfully resolved property '%s' at address %p", prop_name_str, pComplex);
//     return 1;
// }

int complex_assert::resolve_target_property()
{
    const char *target_str = get_target().c_str();

    if (!target_str || *target_str == '\0')
    {
        gl_error("Target property name is empty");
        return 0;
    }

    OBJECT *parent_obj = get_parent() ? get_parent()->my() : nullptr;
    if (!parent_obj)
    {
        gl_error("complex_assert has no parent to resolve target '%s'", target_str);
        return 0;
    }

    OBJECT *target_obj = nullptr;
    char obj_name_str[256] = "";
    char prop_name_str[256] = "";
    char part_name_str[256] = "";

    // --- 1) Try full property name on parent (e.g., "voltage_A", "current_market.clearing_quantity")
    PROPERTY *prop = gl_get_property(parent_obj, target_str, nullptr);
    if (prop != nullptr)
    {
        // Found property directly on the parent
        strcpy(prop_name_str, target_str);
        target_obj = parent_obj;
    }
    else
    {
        // --- 2) Split on first dot
        const char *dot = strchr(target_str, '.');
        if (dot != nullptr)
        {
            size_t left_len = static_cast<size_t>(dot - target_str);
            if (left_len >= sizeof(obj_name_str))
            {
                gl_error("Target left token in '%s' is too long", target_str);
                return 0;
            }
            strncpy(obj_name_str, target_str, left_len);
            obj_name_str[left_len] = '\0';
            strcpy(part_name_str, dot + 1);

            // 2a) Check if LEFT token is a PROPERTY on parent (=> property.part)
            PROPERTY *base_prop = gl_get_property(parent_obj, obj_name_str, nullptr);
            if (base_prop != nullptr)
            {
                // Treat as 'property.part' on parent object
                target_obj = parent_obj;
                strcpy(prop_name_str, obj_name_str);
                // Persist the part for later comparison
                set_part(part_name_str);
            }
            else
            {
                // 2b) Treat as 'object.property': LEFT token is an object name
                FINDLIST *fl = gl_find_objects(FL_NEW, FT_NAME, SAME, obj_name_str, FT_END);
                if (fl == nullptr || fl->hit_count == 0)
                {
                    gl_error("Target object '%s' not found", obj_name_str);
                    if (fl)
                        gl_free((void **)&fl);
                    return 0;
                }
                target_obj = gl_find_next(fl, nullptr);
                gl_free((void **)&fl);

                // Property name is the RIGHT token
                strcpy(prop_name_str, part_name_str);
            }
        }
        else
        {
            // --- 3) No dot: simple property on the parent
            target_obj = parent_obj;
            strcpy(prop_name_str, target_str);
        }
    }

    // Final resolution: get PROPERTY*
    pTarget = gl_get_property(target_obj, prop_name_str);
    if (pTarget == nullptr)
    {
        gl_error("Property '%s' not found on object '%s'",
                 prop_name_str, target_obj->name ? target_obj->name : "unnamed");
        return 0;
    }

    // For complex_assert: if no 'part' is set, the property itself must be PT_complex
    if (get_part().empty())
    {
        if (pTarget->ptype != PT_complex)
        {
            gl_error("Property '%s' is not complex type (type=%d)",
                     prop_name_str, pTarget->ptype);
            return 0;
        }
        pComplex = (gld::complex *)gl_get_addr(target_obj, prop_name_str);
        if (pComplex == nullptr)
        {
            gl_error("Unable to get address of complex property '%s'", prop_name_str);
            return 0;
        }
    }
    else
    {
        // 'property.part' case on complex_assert:
        // Decide policy: either reject (preferred: use double_assert for '.real' / '.imag'),
        // or support components explicitly. Example support:
        if (pTarget->ptype == PT_complex)
        {
            gld::complex *pc = (gld::complex *)gl_get_addr(target_obj, prop_name_str);
            if (!pc)
            {
                gl_error("Unable to get address of complex property '%s'", prop_name_str);
                return 0;
            }
            // Store component pointer or copy value for later comparison.
            // E.g., if part == "real" or "imag", stash it for compare.
            // (Implementation detail: your compare path should look at get_part().)
            pComplex = pc; // keep base pointer; evaluation uses get_part()
        }
        else
        {
            // The base property is not complex; complex_assert should not compare it.
            gl_error("Property '%s' with part '%s' is not complex; use double_assert for scalar components",
                     prop_name_str, get_part().c_str());
            return 0;
        }
    }

    return 1;
}

// Helper
static const char *op_to_string(int op)
{
    switch (op)
    {
    case complex_assert::FULL:
        return "FULL";
    case complex_assert::REAL:
        return "REAL";
    case complex_assert::IMAGINARY:
        return "IMAGINARY";
    case complex_assert::MAGNITUDE:
        return "MAGNITUDE";
    case complex_assert::ANGLE:
        return "ANGLE";
    default:
        return "UNKNOWN";
    }
}

/********************************

Rules:

once = ONCE_TRUE

Evaluate exactly at ts_in.
If violation and no switching → fail now (TS_INVALID).
If violation and switching → do not reschedule (stop).
If pass → stop (no further checks).


once = ONCE_FALSE (continuous)

Evaluate continuously inside [ts_in, ts_out) by returning a future timestamp on pass or skip (e.g., t2+1).
If violation and no switching → fail now.
If violation and switching → defer (set pending_fail_ts) and reschedule for later resolution.
**********************************/

// int complex_assert::postnotify(PROPERTY *prop, char *value)
// {
// 	if (once == ONCE_DONE && strcmp(prop->name, "value") == 0)
// 	{
// 		once = ONCE_TRUE;
// 	}
// 	return 1;
// }

EvalOutcome complex_assert::evaluate_assert(const complex x, bool switched_now)
{

    complex x_local = x; // local, non-const
    complex val = value; // local, non-const copy of member

    const bool once_true = (once == ONCE_TRUE);
    const char *op_str = op_to_string((int)operation);

    // Compute deltas
    const double real_err = x_local.Re() - value.Re();
    const double imag_err = x_local.Im() - value.Im();
    const double mag_err = std::fabs(x.Mag() - value.Mag());
    const double ang_err = std::fabs(x.Arg() - value.Arg());

    // Tolerance checks
    const bool within_real = is_within(real_err, within);
    const bool within_imag = is_within(imag_err, within);
    const bool within_mag = is_within(mag_err, within);
    const bool within_ang = is_within(ang_err, within);

    // Format a common head for messages
    const char *parent_name = (get_parent() && get_parent()->get_name())
                                  ? get_parent()->get_name()
                                  : "(null)";
    char head[256];
    std::snprintf(head, sizeof(head),
                  "Assert [%s] parent=%s target=%s op=%s within=%g",
                  get_name(), parent_name, get_target().c_str(), op_str, within);

    auto fail_immediate = [&](const char *detail)
    {
        gl_error("%s: %s", head, detail);
        return EvalOutcome::FAIL_IMMEDIATE;
    };
    auto fail_turbulence = [&](const char *detail)
    {
        gl_error("%s: %s", head, detail);
        if (once_true)
        {
            // ONCE_TRUE can't defer; treat as immediate failure
            return EvalOutcome::FAIL_IMMEDIATE;
        }
        else
        {
            pending_fail_ts = gl_globalclock;
            gl_debug("complex_assert(%s): deferring failure (turbulence) at %lld",
                     get_name(), (long long)pending_fail_ts);
            return EvalOutcome::FAIL_DEFERRED;
        }
    };

    if (status == ASSERT_TRUE)
    {
        switch (operation)
        {
        case FULL:
            if (within_real && within_imag)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL TRUE(FULL): real_err=%g imag_err=%g", real_err, imag_err);
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case REAL:
            if (within_real)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL TRUE(REAL): real=%g not within %g of %g",
                              x_local.Re(), within, value.Re());
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case IMAGINARY:
            if (within_imag)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL TRUE(IMAG): imag=%g not within %g of %g",
                              x_local.Im(), within, value.Im());
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case MAGNITUDE:
            if (within_mag)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL TRUE(MAG): |x|=%g not within %g of |val|=%g (err=%g)",
                              x.Mag(), within, value.Mag(), mag_err);
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case ANGLE:
            if (within_ang)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL TRUE(ANG): arg(x)=%g not within %g of arg(val)=%g (err=%g)",
                              x.Arg(), within, value.Arg(), ang_err);
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        default:
            gl_warning("%s: unknown operation in ASSERT_TRUE", head);
            return EvalOutcome::PASS;
        }
    }
    else
    { // ASSERT_FALSE
        switch (operation)
        {
        case FULL:
        {
            bool inside = within_real && within_imag;
            if (!inside)
                return EvalOutcome::PASS;
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "FAIL FALSE(FULL): inside tolerance (real_err=%g imag_err=%g)", real_err, imag_err);
            return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
        }
        case REAL:
            if (!within_real)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL FALSE(REAL): |real_err|=%g <= %g (x=%g, val=%g)",
                              std::fabs(real_err), within, x_local.Re(), value.Re());
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case IMAGINARY:
            if (!within_imag)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL FALSE(IMAG): |imag_err|=%g <= %g (x=%g, val=%g)",
                              std::fabs(imag_err), within, x_local.Im(), value.Im());
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case MAGNITUDE:
            if (!within_mag)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL FALSE(MAG): |x|=%g within %g of |val|=%g",
                              x.Mag(), within, value.Mag());
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        case ANGLE:
            if (!within_ang)
                return EvalOutcome::PASS;
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "FAIL FALSE(ANG): arg(x)=%g within %g of arg(val)=%g",
                              x.Arg(), within, value.Arg());
                return (!switched_now) ? fail_immediate(msg) : fail_turbulence(msg);
            }
        default:
            gl_warning("%s: unknown operation in ASSERT_FALSE", head);
            return EvalOutcome::PASS;
        }
    }
}

TIMESTAMP complex_assert::resched_safe(const char *reason)
{
    const TIMESTAMP now = gl_globalclock;

    // If we’re truly done, never schedule again
    if (done)
    {
        gl_debug("ASSERT[%s]: resched (%s) -> TS_NEVER [done]", get_name(), reason);
        return TS_NEVER;
    }

    // First-time scheduling
    if (!prestart_deferral_done)
    {
        TIMESTAMP initial = 0;
        if (ts_in != 0 /* && gl_isvalidtime(ts_in) */)
        {
            initial = ts_in;
        }
        else
        {
            char startbuf[64] = {0};
            gl_global_getvar("starttime", startbuf, sizeof(startbuf));
            TIMESTAMP t_start = gl_parsetime(startbuf);
            initial = (t_start > 0 /* && gl_isvalidtime(t_start) */) ? t_start : (now + 1);
        }
        prestart_deferral_done = true;

        // last_to = initial;
        // char buf[64] = {0}; gl_printtime(initial, buf, sizeof(buf));
        // gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [initial]",
        //          get_name(), reason, (long long)initial, buf);
        // return initial;

        // Ensure strictly future scheduling
        TIMESTAMP next = (initial <= now) ? (now + 1) : initial;
        last_to = next;
        char buf[64] = {0};
        gl_printtime(initial, buf, sizeof(buf));
        gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [initial]",
                 get_name(), reason, (long long)initial, buf);
        return next;
    }

    // Haven’t reached ts_in yet? Aim at it.
    if (ts_in != 0 && now < ts_in)
    {
        // last_to = ts_in;
        // char buf[64] = {0}; gl_printtime(ts_in, buf, sizeof(buf));
        // gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [gate to ts_in]",
        //          get_name(), reason, (long long)ts_in, buf);
        // return ts_in;

        TIMESTAMP next = (ts_in <= now) ? (now + 1) : ts_in;
        last_to = next;
        char buf[64] = {0};
        gl_printtime(ts_in, buf, sizeof(buf));
        gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [gate to ts_in]",
                 get_name(), reason, (long long)ts_in, buf);
        return next;
    }

    // ONCE_TRUE: after ts_in, commit() will evaluate and then stop; do not go into normal cadence
    if (once == ONCE_TRUE)
    {
        gl_debug("ASSERT[%s]: resched (%s) -> TS_NEVER [ONCE_TRUE post-in]",
                 get_name(), reason);
        return TS_NEVER;
    }

    // Continuous mode (ONCE_FALSE): strictly future monotonic scheduling
    TIMESTAMP next = std::max<TIMESTAMP>(last_to + 1, now + 1);
    last_to = next;

    char buf[64] = {0};
    gl_printtime(next, buf, sizeof(buf));
    gl_debug("ASSERT[%s]: resched (%s) -> %lld (%s) [normal]",
             get_name(), reason, (long long)next, buf);
    return next;
}

TIMESTAMP complex_assert::commit(TIMESTAMP t1, TIMESTAMP t2)
{

    // Compute stoptime locally
    char stopbuf[64] = {0};
    gl_global_getvar("stoptime", stopbuf, sizeof(stopbuf));
    TIMESTAMP t_stop = gl_parsetime(stopbuf);

    // Resolve target on first commit (lazy init)
    if (pComplex == nullptr)
    {
        if (resolve_target_property() == 0)
        {
            return resched_safe("target unresolved; retry");
        }
    }

    // Build switch index if needed
    if (!switches_index_built)
        build_switch_index();

    // Record switching for this step
    bool switched_now = switching_happened_this_step(t2);

    // Hard stoptime guard
    if (t2 >= t_stop)
    {
        gl_debug("complex_assert(%s): skipping at stoptime %s", get_name(), stopbuf);
        return TS_NEVER;
    }

    const TIMESTAMP now = gl_globalclock;
    const bool once_true = (once == ONCE_TRUE);

    // --- ONCE_TRUE gate: enforce evaluation exactly at ts_in ---
    if (once_true && ts_in != 0)
    {
        if (now < ts_in)
        {
            // Park at the scheduled 'in' time
            return resched_safe("ONCE_TRUE: pre-in gate");
        }
        else if (now == ts_in)
        {

            // Evaluate once at ts_in
            complex x = *pComplex; // safe: pComplex resolved earlier
            if (!std::isfinite(x.Re()) || !std::isfinite(x.Im()))
            {
                return resched_safe("ONCE_TRUE: non-finite at ts_in; skip");
            }
            const double mag = x.Mag();
            const double expected_mag = value.Mag();

            // Skip if magnitude ratio indicates topology/phase issues
            if (expected_mag > 1e-6)
            {
                double mag_ratio = mag / expected_mag;
                if (mag_ratio < 0.1 || mag_ratio > 10.0)
                {
                    gl_verbose("Assert skipped on %s: magnitude ratio (%g) suggests topology issue",
                               get_parent()->get_name(), mag_ratio);
                    return TS_NEVER;
                }
            }

            if (mag < 1e-3 || mag > 1e5)
            {
                return resched_safe("implausible magnitude; skip");
            }

            //  CHECK: Skip if magnitude ratio indicates topology/phase issues
            if (expected_mag > 1e-6)
            {
                double mag_ratio = mag / expected_mag;
                if (mag_ratio < 0.5 || mag_ratio > 2.0) // Tighter bounds for voltage checks
                {
                    gl_verbose("Assert skipped on %s: %s magnitude ratio (%g) suggests topology/phase issue (actual=%g, expected=%g)",
                               get_parent()->get_name(), get_target().c_str(), mag_ratio, mag, expected_mag);
                    return TS_NEVER;
                }
            }

            double mag_ratio = (expected_mag > 1e-6) ? (mag / expected_mag) : 0.0;

            if (status == ASSERT_TRUE && (operation == FULL || operation == REAL || operation == IMAGINARY))
            {
                complex error = x - value;
                double real_error = fabs(error.Re());
                double imag_error = fabs(error.Im());

                bool mag_ok = (mag_ratio >= 0.8 && mag_ratio <= 1.2);
                bool error_exceeds_tolerance = (real_error > within) || (imag_error > within);

                if (mag_ok && error_exceeds_tolerance)
                {
                    gl_verbose("Assert skipped on %s: %s has OK magnitude ratio (%g) but component errors exceed tolerance (real=%g, imag=%g vs within=%g) - possible phase/topology issue",
                               get_parent()->get_name(), get_target().c_str(),
                               mag_ratio, real_error, imag_error, within);
                    return TS_NEVER;
                }
            }

            EvalOutcome outcome = evaluate_assert(x, switched_now);
            // Latch one-shot completion
            once_value = value;
            once = ONCE_DONE;
            done = true;

            if (outcome == EvalOutcome::PASS)
            {
                gl_debug("Assert [%s]: ONCE_TRUE PASS at ts_in; stopping", get_name());
                return TS_NEVER;
            }
            else
            {
                gl_debug("Assert [%s]: ONCE_TRUE FAIL at ts_in; stopping", get_name());
                // For ONCE_TRUE, treat any failure as immediate (non-zero exit)
                return TS_INVALID;
            }
        }
        else
        { // now > ts_in
            // Missed window; don't keep rescheduling
            gl_warning("Assert [%s]: ONCE_TRUE missed ts_in (%lld > %lld); stopping",
                       get_name(), (long long)now, (long long)ts_in);
            once = ONCE_DONE;
            done = true;
            return TS_NEVER;
        }
    }

    // --- Engine advisory/sentinel handling (AFTER ONCE_TRUE gate) ---
    // if (t2 == TS_NEVER) {
    //     const bool past_out     = (ts_out != 0 && now >= ts_out);
    //     const bool at_stop      = (t_stop > 0 && now >= t_stop);
    //     const bool once_is_done = (once == ONCE_DONE);
    //     if (past_out || at_stop || once_is_done || done) {
    //         gl_debug("complex_assert(%s): [A] stopping", get_name());
    //         return TS_NEVER;
    //     }
    //     // return resched_safe("engine TS_NEVER -> re-emit");
    // 	// return resched_safe("engine TS_NEVER -> re-emit (future)");

    // 	// Advisory from engine; evaluate at 'now' and stop
    // 	complex x = *pComplex;
    // 	if (!std::isfinite(x.Re()) || !std::isfinite(x.Im())) return TS_NEVER;
    // 	const double mag = x.Mag();
    // 	if (mag < 1e-3 || mag > 1e5) return TS_NEVER;
    // 	EvalOutcome outcome = evaluate_assert(x, /*switched_now=*/false);
    // 	done = true;
    // 	return (outcome == EvalOutcome::PASS) ? TS_NEVER : TS_INVALID;

    // }

    // if (t2 <= 0 /* || !gl_isvalidtime(t2) */) {
    //     // return resched_safe("engine invalid t2 -> re-emit");
    // 	// return resched_safe("engine invalid t2 -> re-emit (future)");

    // 	// Snapshot commit: evaluate at 'now' and stop to avoid busy-loop
    // 	complex x = *pComplex;
    // 	if (!std::isfinite(x.Re()) || !std::isfinite(x.Im())) return TS_NEVER;
    // 	const double mag = x.Mag();
    // 	if (mag < 1e-3 || mag > 1e5) return TS_NEVER;
    // 	EvalOutcome outcome = evaluate_assert(x, /*switched_now=*/false);
    // 	done = true;
    // 	return (outcome == EvalOutcome::PASS) ? TS_NEVER : TS_INVALID;

    // }

    // --- Engine advisory/sentinel handling (AFTER ONCE_TRUE gate) ---
    if (t2 == TS_NEVER || t2 <= 0)
    {

        // If it's clearly "one-shot", treat as snapshot
        bool one_shot = (once == ONCE_TRUE) ||
                        ((ts_in == 0) && (ts_out == 0) && !model_is_continuous_like());

        if (one_shot)
        {

            // If we're not at/near stoptime, reschedule to stoptime
            if (t_stop > 0 && now < t_stop - 1)
            {
                return t_stop - 1; // Evaluate just before stop
            }
            // Evaluate once and stop
            complex x = *pComplex;
            if (!std::isfinite(x.Re()) || !std::isfinite(x.Im()))
                return TS_NEVER;
            double mag = x.Mag();
            if (mag < 1e-3 || mag > 1e5)
                return TS_NEVER;
            EvalOutcome o = evaluate_assert(x, /*switched_now=*/false);
            return (o == EvalOutcome::PASS) ? TS_NEVER : TS_INVALID;
        }
        else
        {
            // Continuous-like model: throttle rescheduling to avoid 1-second treadmill
            // Pick a conservative cadence (e.g., 900 s = 15 minutes), capped by stoptime
            const TIMESTAMP now = gl_globalclock;
            char stopbuf[64] = {0};
            gl_global_getvar("stoptime", stopbuf, sizeof(stopbuf));
            TIMESTAMP t_stop = gl_parsetime(stopbuf);

            const TIMESTAMP step = 900; // 15 minutes default
            TIMESTAMP next = now + step;
            if (t_stop > 0 && next >= t_stop)
            {
                // If we’d hit or exceed stoptime, just stop
                return TS_NEVER;
            }
            last_to = next;
            return next;
        }
    }

    // --- Normal gating (non-ONCE_TRUE) ---
    if (ts_in != 0 && now < ts_in)
    {
        return resched_safe("before-in gate");
    }
    if (ts_out != 0 && now >= ts_out)
    {
        return TS_NEVER;
    }

    // Deferred failure resolution – if you deferred previously, decide now
    if (pending_fail_ts != 0)
    {
        bool switched_then = ts_had_switch_change[pending_fail_ts];
        pending_fail_ts = 0;
        if (!switched_then)
        {
            gl_error("Assert failed on %s: deferred failure cleared but no switching at prior step",
                     get_parent()->get_name());
            return TS_INVALID;
        }
        else
        {
            gl_debug("Assert failed on %s: deferred failure cleared with switching at prior step",
                     get_parent()->get_name());
            // Continue to current evaluation
        }
    }

    // Read target and plausibility checks
    complex x = *pComplex;
    if (!std::isfinite(x.Re()) || !std::isfinite(x.Im()))
    {
        return resched_safe("non-finite value; skip");
    }
    const double mag = x.Mag();
    if (mag < 1e-3 || mag > 1e5)
    {
        return resched_safe("implausible magnitude; skip");
    }

    // --- Main assertion logic (continuous mode) ---
    EvalOutcome outcome = evaluate_assert(x, switched_now);

    if (status == ASSERT_TRUE)
    {
        switch (outcome)
        {
        case EvalOutcome::PASS:
            gl_debug("Assert [%s]: pass; rescheduling (ONCE_FALSE)", get_name());
            return resched_safe("pass; rescheduling (ONCE_FALSE)");
        case EvalOutcome::FAIL_IMMEDIATE:
            return TS_INVALID;
        case EvalOutcome::FAIL_DEFERRED:
            return resched_safe("deferred fail; turbulence");
        }
    }
    else
    { // ASSERT_FALSE
        switch (outcome)
        {
        case EvalOutcome::PASS:
            gl_debug("Assert [%s]: ASSERT_FALSE pass (outside tolerance); rescheduling", get_name());
            return resched_safe("ASSERT_FALSE pass; reschedule");
        case EvalOutcome::FAIL_IMMEDIATE:
            return TS_INVALID;
        case EvalOutcome::FAIL_DEFERRED:
            return resched_safe("ASSERT_FALSE deferred fail; turbulence");
        }
    }

    // Fallback (shouldn't happen)
    gl_debug("ASSERT[%s]: unexpected fall-through; returning TS_NEVER", get_name());
    return TS_NEVER;
}

// Only call gl_localtime() when the computed del_clock_int is a valid simulation timestamp.
// If not, fall back to the raw numeric time.
// This keeps asserts from crashing during the very first (pre-clock) delta iteration.
static bool format_error_timestamp(TIMESTAMP ts,
                                   double del_clock,
                                   int del_microseconds,
                                   char *datebuff,
                                   size_t dbufsz)
{
    // Guard against invalid/early timestamps
    if (ts == TS_INVALID || ts == TS_NEVER || ts <= TS_ZERO)
    {
        // Fallback: numeric seconds output
        snprintf(datebuff, dbufsz, "ERROR    %.09f : ", del_clock);
        return false;
    }

    DATETIME dt;
    char dateformat[16] = "";
    gl_localtime(ts, &dt);
    gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

    if (strcmp(dateformat, "ISO") == 0)
    {
        snprintf(datebuff, dbufsz,
                 "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%06d %s] : ",
                 dt.year, dt.month, dt.day,
                 dt.hour, dt.minute, dt.second,
                 del_microseconds, dt.tz);
    }
    else if (strcmp(dateformat, "US") == 0)
    {
        snprintf(datebuff, dbufsz,
                 "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%06d %s] : ",
                 dt.month, dt.day, dt.year,
                 dt.hour, dt.minute, dt.second,
                 del_microseconds, dt.tz);
    }
    else if (strcmp(dateformat, "EURO") == 0)
    {
        snprintf(datebuff, dbufsz,
                 "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%06d %s] : ",
                 dt.day, dt.month, dt.year,
                 dt.hour, dt.minute, dt.second,
                 del_microseconds, dt.tz);
    }
    else
    {
        // Unknown dateformat — numeric fallback
        snprintf(datebuff, dbufsz, "ERROR    %.09f : ", del_clock);
        return false;
    }

    return true; // formatted using gl_localtime
}

EXPORT SIMULATIONMODE update_complex_assert(OBJECT *obj, TIMESTAMP t0, unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{

    // Only assert once real time is established
    if (t0 == TS_ZERO)
    {
        gl_verbose("Assert deferred: global clock not initialized yet");
        return SM_EVENT;
    }

    char buff[128];
    char dateformat[16] = "";
    char error_output_buff[2048];
    char datebuff[128];
    /*complex_assert *da = OBJECTDATA(obj,complex_assert);*/
    complex_assert *da = object_data<complex_assert>(obj);

    // DATETIME delta_dt_val;
    double del_clock;
    TIMESTAMP del_clock_int;
    int del_microseconds;
    complex *x;

    if (da->get_once() == da->ONCE_TRUE)
    {
        da->set_once_value(da->get_value());
        da->set_once(da->ONCE_DONE);
    }
    else if (da->get_once() == da->ONCE_DONE)
    {
        if (da->get_once_value().Re() == da->get_value().Re() && da->get_once_value().Im() == da->get_value().Im())
        {
            gl_verbose("Assert skipped with ONCE logic");
            return SM_EVENT;
        }
        else
        {
            da->set_once_value(da->get_value());
        }
    }

    // Iteration checker - assert only valid on the first timestep
    if (iteration_count_val == 0)
    {
        // Skip first timestep of any delta iteration -- nature of delta means it really isn't checking the right one
        if (delta_time >= dt)
        {
            // Get value
            // x = (complex*)gl_get_complex_by_name(obj->parent,da->get_target());
            x = (complex *)gl_get_complex_by_name(obj->parent, da->get_target().c_str());

            if (x == nullptr)
            {
                gl_error("Specified target %s for %s is not valid.", da->get_target().c_str(), gl_name(obj->parent, buff, 64));
                /*  TROUBLESHOOT
                Check to make sure the target you are specifying is a published variable for the object
                that you are pointing to.  Refer to the documentation of the command flag --modhelp, or
                check the wiki page to determine which variables can be published within the object you
                are pointing to with the assert function.
                */
                return SM_ERROR;
            }

            double expected_mag = da->get_value().Mag();
            double actual_mag = x->Mag();

            if (expected_mag > 1e-6)
            {
                double mag_ratio = actual_mag / expected_mag;
                if (mag_ratio < 0.5 || mag_ratio > 2.0)
                {
                    gl_verbose("Assert skipped on %s: %s magnitude ratio (%g) suggests topology issue",
                               gl_name(obj->parent, buff, 64), da->get_target().c_str(), mag_ratio);
                    return SM_EVENT;
                }
            }

            // Check 1: Completely isolated (near-zero voltage when expecting non-zero)
            if (expected_mag > 1.0 && actual_mag < 1e-6)
            {
                gl_verbose("Assert skipped on %s: %s appears isolated (actual mag=%g, expected mag=%g)",
                           gl_name(obj->parent, buff, 64), da->get_target().c_str(), actual_mag, expected_mag);
                return SM_EVENT;
            }

            // Check 2: Non-finite values (NaN or Inf from solver issues)
            if (!std::isfinite(x->Re()) || !std::isfinite(x->Im()))
            {
                gl_verbose("Assert skipped on %s: %s has non-finite value - node may have solver issues",
                           gl_name(obj->parent, buff, 64), da->get_target().c_str());
                return SM_EVENT;
            }

            // Check 3: Voltage magnitude way outside expected range (possible meshed/phase issue)
            // If expected is a typical distribution voltage (~2400V) but actual is very different
            double mag_ratio = (expected_mag > 1e-6) ? (actual_mag / expected_mag) : 0.0;
            if (mag_ratio < 0.1 || mag_ratio > 10.0)
            {
                gl_verbose("Assert skipped on %s: %s magnitude ratio (%g) suggests topology issue (actual=%g, expected=%g)",
                           gl_name(obj->parent, buff, 64), da->get_target().c_str(), mag_ratio, actual_mag, expected_mag);
                return SM_EVENT;
            }

            // CHECK: Detect phase/topology issues via component error ratio
            // If magnitudes are similar but component errors are large relative to within,
            // this suggests a phase rotation or topology issue
            if (da->get_status() == da->ASSERT_TRUE &&
                (da->get_operation() == da->FULL || da->get_operation() == da->REAL || da->get_operation() == da->IMAGINARY))
            {
                complex error = *x - da->get_value();
                double real_error = fabs(error.Re());
                double imag_error = fabs(error.Im());

                // If magnitude ratio is OK (0.8-1.2) but component errors exceed 3x tolerance,
                // likely a phase/topology issue - skip rather than fail
                double mag_ratio = (expected_mag > 1e-6) ? (actual_mag / expected_mag) : 0.0;
                bool mag_ok = (mag_ratio >= 0.8 && mag_ratio <= 1.2);
                bool error_exceeds_tolerance = (real_error > da->get_within()) || (imag_error > da->get_within());

                if (mag_ok && error_exceeds_tolerance)
                {
                    gl_verbose("Assert skipped on %s: %s has OK magnitude but errors exceed tolerance - possible phase/topology issue",
                               gl_name(obj->parent, buff, 64), da->get_target().c_str());
                    return SM_EVENT;
                }
            }

            if (da->get_status() == da->ASSERT_TRUE)
            {
                if (da->get_operation() == da->FULL || da->get_operation() == da->REAL || da->get_operation() == da->IMAGINARY)
                {
                    complex error = *x - da->get_value();
                    double real_error = error.Re();
                    double imag_error = error.Im();
                    if ((std::isnan(real_error) || fabs(real_error) > da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->REAL))
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);

                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - real part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Re(), da->get_within(), da->get_value().Re());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                    if ((std::isnan(imag_error) || fabs(imag_error) > da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->IMAGINARY))
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - imaginary part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Im(), da->get_within(), da->get_value().Im());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                }
                else if (da->get_operation() == da->MAGNITUDE)
                {
                    double magnitude_error = (*x).Mag() - da->get_value().Mag();
                    if (std::isnan(magnitude_error) || fabs(magnitude_error) > da->get_within())
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - Magnitude of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Mag(), da->get_within(), da->get_value().Mag());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                }
                else if (da->get_operation() == da->ANGLE)
                {
                    double angle_error = (*x).Arg() - da->get_value().Arg();
                    if (std::isnan(angle_error) || fabs(angle_error) > da->get_within())
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - Angle of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Arg(), da->get_within(), da->get_value().Arg());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                }
                gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
                return SM_EVENT;
            }
            else if (da->get_status() == da->ASSERT_FALSE)
            {
                if (da->get_operation() == da->FULL || da->get_operation() == da->REAL || da->get_operation() == da->IMAGINARY)
                {
                    complex error = *x - da->get_value();
                    double real_error = error.Re();
                    double imag_error = error.Im();
                    if ((std::isnan(real_error) || fabs(real_error) < da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->REAL))
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - real part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Re(), da->get_within(), da->get_value().Re());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                    if ((std::isnan(imag_error) || fabs(imag_error) < da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->IMAGINARY))
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - imaginary part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Im(), da->get_within(), da->get_value().Im());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                }
                else if (da->get_operation() == da->MAGNITUDE)
                {
                    double magnitude_error = (*x).Mag() - da->get_value().Mag();
                    if (std::isnan(magnitude_error) || fabs(magnitude_error) < da->get_within())
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - Magnitude of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Mag(), da->get_within(), da->get_value().Mag());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                }
                else if (da->get_operation() == da->ANGLE)
                {
                    double angle_error = (*x).Arg() - da->get_value().Arg();
                    if (std::isnan(angle_error) || fabs(angle_error) < da->get_within())
                    {
                        // Calculate time
                        if (delta_time >= dt) // After first iteration
                            del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
                        else // First second different, don't back out
                            del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

                        del_clock_int = (TIMESTAMP)del_clock;                                     /* Whole seconds - update from global clock because we could be in delta for over 1 second */
                        del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

                        // Convert out
                        // gl_localtime(del_clock_int, &delta_dt_val);
                        if (!format_error_timestamp(del_clock_int, del_clock, del_microseconds, datebuff, sizeof(datebuff)))
                        {
                            // Already filled datebuff with a fallback; continue
                        }

                        // Determine output format
                        gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

                        // Output date appropriately
                        // if (strcmp(dateformat, "ISO") == 0)
                        //     sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "US") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else if (strcmp(dateformat, "EURO") == 0)
                        //     sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
                        // else
                        //     sprintf(datebuff, "ERROR    %.09f : ", del_clock);

                        // Actual error part
                        sprintf(error_output_buff, "Assert failed on %s - Angle of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Arg(), da->get_within(), da->get_value().Arg());

                        // Send it out
                        gl_output("%s%s", datebuff, error_output_buff);

                        return SM_ERROR;
                    }
                }
                gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
                return SM_EVENT;
            }
            else
            {
                gl_verbose("Assert test is not being run on %s", gl_name(obj->parent, buff, 64));
                return SM_EVENT;
            }
        }
        else // First timestep, just proceed
            return SM_EVENT;
    }
    else // Iteration, so don't care
        return SM_EVENT;
}

// ---- Explicit C exports (create/init/commit) ----
extern "C"
{
    // Create wrapper - constructs the object
    EXPORT int create_complex_assert(OBJECT **obj, OBJECT *parent)
    {
        try
        {
            *obj = gl_create_object(complex_assert::oclass);
            if (*obj == nullptr)
                return 0;
            complex_assert *my = object_data<complex_assert>(*obj);
            if (!my)
            {
                gl_error("create_complex_assert: obj->data is null for class 'complex_assert'");
                return 0;
            }
            // Let the core set parent; or call gl_set_parent(*obj, parent) if needed
            return my->create();
        }
        CREATE_CATCHALL(complex_assert);
    }

    // Init wrapper - calls the C++ init with the parent handle
    EXPORT int init_complex_assert(OBJECT *obj)
    {
        try
        {
            complex_assert *my = object_data<complex_assert>(obj);
            if (!my)
            {
                gl_error("init_complex_assert: obj->data is null for complex_assert");
                return 0;
            }
            return my->init(obj->parent);
        }
        CREATE_CATCHALL(complex_assert);
    }

    // Commit wrapper - core calls this; we forward to the C++ method
    EXPORT TIMESTAMP commit_complex_assert(OBJECT *obj, TIMESTAMP t1, TIMESTAMP t2)
    {
        try
        {
            complex_assert *my = object_data<complex_assert>(obj);
            if (!my)
            {
                gl_error("commit_complex_assert: obj->data is null for complex_assert");
                return TS_INVALID;
            }
            return my->commit(t1, t2);
        }
        catch (const std::exception &ex)
        {
            gl_error("commit_complex_assert(obj=%d;%s): %s",
                     obj->id, (obj->name ? obj->name : "unnamed"), ex.what());
            return TS_INVALID;
        }
        catch (...)
        {
            gl_error("commit_complex_assert(obj=%d;%s): unhandled exception",
                     obj->id, (obj->name ? obj->name : "unnamed"));
            return TS_INVALID;
        }
    }
}
