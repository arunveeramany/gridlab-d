/** $Id: enduse.c 4738 2014-07-03 00:55:39Z dchassin $
 	Copyright (C) 2008 Battelle Memorial Institute
	@file loadshape.c
	@addtogroup loadshape
**/

// #include <cctype>
// #include <cmath>
// #include <cstdarg>
// #include <cstdlib>
// //#include <pthread.h>

// #include <thread>
// #include <mutex>
// #include <condition_variable>
// #include <shared_mutex>
// #include <chrono>

// #include "platform.h"
// #include "output.h"
// #include "loadshape.h"
// #include "exception.h"
// #include "convert.h"
// #include "globals.h"
// #include "gldrandom.h"
// #include "schedule.h"
// #include "enduse.h"
// #include "gridlabd.h"
// #include "exec.h"
// #include "gld_complex.h"


// Core GridLAB-D and C++ class headers first
#include "gridlabd.h"
#include "enduse.h"
#include "class.h"  

// Standard library includes
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <chrono>

// Other GridLAB-D module headers
#include "platform.h"
#include "output.h"
#include "loadshape.h"
#include "exception.h"
#include "convert.h"
#include "globals.h"
#include "gldrandom.h"
#include "schedule.h"
#include "exec.h"
#include "gld_complex.h"


// static enduse *enduse_list = nullptr;
CLASS* enduse::oclass = nullptr;
CLASS* enduse::pclass = nullptr;
extern "C" CALLBACKS *callback;
static unsigned int n_enduses = 0;

enduse* enduse::defaults = nullptr;

double enduse_get_part(void *x, const char *name)
{
    // std::cerr << "enduse_get_part called with name='" << name << "'" << std::endl;
	enduse *e = (enduse*)x;
    gl_warning("enduse_get_part called with name='%s', e->power=(%g,%g), e->total=(%g,%g)",
        name,
        e->power.Re(), e->power.Im(),
        e->total.Re(), e->total.Im());

#define _DO_DOUBLE(X,Y) if ( strcmp(name,Y)==0) return e->X;
#define _DO_COMPLEX(X,Y) \
	if ( strcmp(name,Y".real")==0) return e->X.Re(); \
	if ( strcmp(name,Y".imag")==0) return e->X.Im(); \
	if ( strcmp(name,Y".mag")==0) return e->X.Mag(); \
	if ( strcmp(name,Y".arg")==0) return e->X.Arg(); \
	if ( strcmp(name,Y".ang")==0) return e->X.Arg()*180/PI;
#define DO_DOUBLE(X) _DO_DOUBLE(X,#X)
#define DO_COMPLEX(X) _DO_COMPLEX(X,#X)
	DO_COMPLEX(total);
	DO_COMPLEX(energy);
	DO_COMPLEX(demand);
	DO_DOUBLE(breaker_amps);
	DO_COMPLEX(admittance);
	DO_COMPLEX(current);
	DO_COMPLEX(power);
	DO_DOUBLE(impedance_fraction);
	DO_DOUBLE(current_fraction);
	DO_DOUBLE(power_fraction);
	DO_DOUBLE(power_factor);
	DO_DOUBLE(voltage_factor);
	DO_DOUBLE(heatgain);
	DO_DOUBLE(heatgain_fraction);
#define DO_MOTOR(X) \
	_DO_COMPLEX(motor[EUMT_MOTOR_##X].power,"motor"#X".power"); \
	_DO_COMPLEX(motor[EUMT_MOTOR_##X].impedance,"motor"#X".impedance"); \
	_DO_DOUBLE(motor[EUMT_MOTOR_##X].inertia,"motor"#X".inertia"); \
	_DO_DOUBLE(motor[EUMT_MOTOR_##X].v_stall,"motor"#X".v_stall"); \
	_DO_DOUBLE(motor[EUMT_MOTOR_##X].v_start,"motor"#X".v_start"); \
	_DO_DOUBLE(motor[EUMT_MOTOR_##X].v_trip,"motor"#X".v_trip"); \
	_DO_DOUBLE(motor[EUMT_MOTOR_##X].t_trip,"motor"#X".t_trip");
	DO_MOTOR(A);
	DO_MOTOR(B);
	DO_MOTOR(C);
	DO_MOTOR(D);
#define DO_ELECTRONIC(X) \
	_DO_COMPLEX(electronic[EUMT_MOTOR_##X].power,"electronic"#X".power"); \
	_DO_DOUBLE(electronic[EUMT_MOTOR_##X].inertia,"electronic"#X".inertia"); \
	_DO_DOUBLE(electronic[EUMT_MOTOR_##X].v_trip,"electronic"#X".v_trip"); \
	_DO_DOUBLE(electronic[EUMT_MOTOR_##X].v_start,"electronic"#X".v_start");
	DO_ELECTRONIC(A);
	DO_ELECTRONIC(B);
	return QNAN;
}

#ifdef _DEBUG
static unsigned int enduse_magic = 0x8c3d7762;
#endif


// Add the new constructor implementation
enduse::enduse(MODULE *mod) 
{
    // first time init
    if (oclass==nullptr)
    {
        // register the class and properties
        // oclass = gl_register_class(mod,"enduse",sizeof(enduse),PC_AUTOLOCK|PC_POSTTOPDOWN);
		// oclass = class_register(mod,"enduse",sizeof(enduse),PC_AUTOLOCK|PC_POSTTOPDOWN);
		oclass = gld_class::create(mod, "enduse", sizeof(enduse), PC_AUTOLOCK|PC_POSTTOPDOWN);

		
        if (oclass==nullptr)
            throw "unable to register class enduse";

		// Use function-local static to avoid initialization order issues
        static enduse local_defaults_storage;
        enduse::defaults = &local_defaults_storage;
        
        // Initialize defaults
        defaults->total       = gld::complex(0.0, 0.0);
        defaults->energy      = gld::complex(0.0, 0.0);
        defaults->demand      = gld::complex(0.0, 0.0);
        defaults->impedance_fraction = 0.0;
        defaults->current_fraction   = 0.0;
        defaults->power_fraction     = 0.0;
        defaults->power_factor       = 1.0;
        defaults->voltage_factor     = 1.0;
        defaults->breaker_amps       = 0.0;
        defaults->heatgain           = 0.0;
        defaults->heatgain_fraction  = 0.0;
        defaults->shape              = nullptr;

		if (!callback || !callback->define_map) {
    		GL_THROW("enduse: callback/define_map not initialized before publish");
		}


        // publish the properties
        if (gl_publish_variable(oclass,
            // Use PADDR macro to get member offsets
            // PT_complex, "power[kW]", PADDR(power),
            //PT_complex, "power",    (ptrdiff_t)PADDR(total),      //"the total power consumption", "kVA", nullptr, 0},
            PT_complex, "constant_power", PADDR(power), //"the constant power (P) component", "kW", nullptr, 0},
            PT_complex, "total[kW]", PADDR(total),
            PT_complex, "energy[kWh]", PADDR(energy),
            PT_complex, "demand[kW]", PADDR(demand),
            PT_set, "config", PADDR(config),
                PT_KEYWORD, "IS110", (int64)EUC_IS110,
                PT_KEYWORD, "IS220", (int64)EUC_IS220,
                PT_KEYWORD, "HEATLOAD", (int64)EUC_HEATLOAD,
            PT_double, "breaker_amps[A]", PADDR(breaker_amps),
            PT_complex, "admittance[kW]", PADDR(admittance),
            PT_complex, "current[kW]", PADDR(current),
            PT_double, "impedance_fraction", PADDR(impedance_fraction),
            PT_double, "current_fraction", PADDR(current_fraction),
            PT_double, "power_fraction", PADDR(power_fraction),
            PT_double, "power_factor", PADDR(power_factor),
            PT_double, "voltage_factor", PADDR(voltage_factor),
            PT_double, "heatgain[Btu/h]", PADDR(heatgain),
            PT_double, "heatgain_fraction", PADDR(heatgain_fraction),
            PT_object, "shape", PADDR(shape),
            NULL)<1)
                throw "unable to publish enduse properties";

		

    }


   
	

}

// int enduse_create(enduse *data)
// {
// 	memset(data,0,sizeof(enduse));
// 	data->next = enduse_list;
// 	enduse_list = data;
// 	n_enduses++;

// 	// check the power factor
// 	data->power_factor = 1.0;
// 	data->heatgain_fraction = 1.0;

// #ifdef _DEBUG
// 	data->magic = enduse_magic;
// #endif
// 	return 1;
// }

// int enduse_init(enduse *e)
// {
// #ifdef _DEBUG
// 	if (e->magic!=enduse_magic)
// 		throw_exception("enduse '%s' magic number bad", e->name);
// #endif

// 	e->t_last = TS_ZERO;

// 	return 0;
// }


int enduse::create(void)
{
    // Logic from old enduse_create()
    power_factor = 1.0;
    heatgain_fraction = 1.0;
	total = gld::complex(0.0, 0.0);
	energy = gld::complex(0.0, 0.0);
	demand = gld::complex(0.0, 0.0);
	impedance_fraction = 0.0;
	current_fraction = 0.0;
	power_fraction = 0.0;
	breaker_amps = 0.0;
	heatgain = 0.0;
	cumulative_heatgain = 0.0;
	shape = nullptr;
    return 1; // Success
}

int enduse::init(OBJECT *parent)
{
    // Logic from old enduse_init()
    t_last = TS_ZERO;
    return 0; // Success
}

// int enduse_initall(void)
// {
// 	enduse *e;
// 	for (e=enduse_list; e!=nullptr; e=e->next)
// 	{
// 		if (enduse_init(e)==1)
// 			return FAILED;
// 	}
// 	return SUCCESS;
// }

//  TIMESTAMP enduse_sync(enduse *e, PASSCONFIG pass, TIMESTAMP t1)
//  {

extern "C" TIMESTAMP enduse_sync(void *obj, ...)
{
    va_list args;
    va_start(args, obj);
    TIMESTAMP t1 = va_arg(args, TIMESTAMP);
    PASSCONFIG pass = va_arg(args, PASSCONFIG);
    va_end(args);

	OBJECT *p_obj = (OBJECT*)obj;
    enduse *e = object_data<enduse>(p_obj); 
	
	if (!callback) {
        gl_error("callback is null in enduse_sync");
        return 0;  // Fail module load
    }


	if (!callback->time.local_datetime) {
        gl_error("CRITICAL: local_datetime callback is null in pass %d", pass);
        return FAILED;
    }



#ifdef _DEBUG
	if (e->magic!=enduse_magic)
		throw_exception("enduse '%s' magic number bad", e->name);
#endif

	if (pass==PC_PRETOPDOWN)// && t1>e->t_last)
	{
		if (e->t_last>TS_ZERO)
		{
			double dt = (double)(t1-e->t_last)/(double)3600;
			e->energy.Re() += e->total.Re() * dt;
			e->energy.Im() += e->total.Im() * dt;
			e->cumulative_heatgain += e->heatgain * dt;
			if(dt > 0.0)
				e->heatgain = 0; /* heat is a dt thing, so dt=0 -> Q*dt = 0 */
		}
		e->t_last = t1;
	}
	else if(pass==PC_BOTTOMUP)
	{
		if (e->shape && e->shape->type != MT_UNKNOWN) // shape driven -> use fractions
		{
			// non-electric load
			if (e->config&EUC_HEATLOAD)
			{
				e->heatgain = e->shape->load;
			}

			// electric load
			else
			{
				double P = e->voltage_factor>0 ? e->shape->load * (e->power_fraction + e->current_fraction + e->impedance_fraction) : 0.0;
				e->total.Re() = P;
				if (fabs(e->power_factor)<1)
					e->total.Im() = (e->power_factor<0?-1:1)*P*sqrt(1/(e->power_factor*e->power_factor)-1);
				else
					e->total.Im() = 0;

				// beware: these are misnomers (they are e->constant_power, e->constant_current, ...)
				e->power.Re() = e->total.Re() * e->power_fraction; e->power.Im() = e->total.Im() * e->power_fraction;
				e->current.Re() = e->total.Re() * e->current_fraction; e->current.Im() = e->total.Im() * e->current_fraction;
				e->admittance.Re() = e->total.Re() * e->impedance_fraction; e->admittance.Im() = e->total.Im() * e->impedance_fraction;
			}
		}
		else if (e->voltage_factor > 0 && !(e->config&EUC_HEATLOAD)) // no shape electric - use ZIP component directly
		{
			e->total.Re() = e->power.Re() + e->current.Re() + e->admittance.Re();
			e->total.Im() = e->power.Im() + e->current.Im() + e->admittance.Im() ;
		}
		else
		{
			/* don't touch anything */
		}

		// non-electric load
		if (e->config&EUC_HEATLOAD)
		{
			e->heatgain *= e->heatgain_fraction;
		}

		// electric load
		else
		{
			if (e->total.Re() > e->demand.Re()) e->demand = e->total;
			if(e->heatgain_fraction > 0.0)
				e->heatgain = e->total.Re() * e->heatgain_fraction * 3412.1416 /* Btu/h/kW */;
		}

		e->t_last = t1;
	}
	return (e->shape && e->shape->type != MT_UNKNOWN) ? e->shape->t2 : TS_NEVER;
}


/**
 * @brief Synchronizes the enduse object with the simulation time.
 * @param t0 The start time of the current time step.
 * @param t1 The end time of the current time step.
 * @return The timestamp of the next required update, or TS_NEVER.
 */
// TIMESTAMP enduse::postsync(TIMESTAMP t0, TIMESTAMP t1)
// {
//     // --- Energy and heat accumulation logic (previously in PC_PRETOPDOWN) ---
//     if (t_last > TS_ZERO)
//     {
//         // Calculate time delta in hours since the last update
//         double dt = (double)(t1 - t_last) / 3600.0;
        
//         // Accumulate energy based on the total power from the previous step
//         energy.Re() += total.Re() * dt;
//         energy.Im() += total.Im() * dt;
        
//         // Accumulate heatgain
//         cumulative_heatgain += heatgain * dt;

//         // Reset instantaneous heatgain for this step (it will be recalculated below)
//         if (dt > 0.0)
//         {
//             heatgain = 0.0;
//         }
//     }

//     // --- Load calculation logic (previously in PC_BOTTOMUP) ---
//     // This updates the ZIP components from the total load, or vice-versa.
//     if (shape && shape->type != MT_UNKNOWN) // Case 1: Load is driven by a loadshape
//     {
//         // 'total' load is set by the loadshape. Now, break it down into ZIP components.
//         power.Re() = total.Re() * power_fraction;
//         power.Im() = total.Im() * power_fraction;

//         current.Re() = total.Re() * current_fraction;
//         current.Im() = total.Im() * current_fraction;

//         admittance.Re() = total.Re() * impedance_fraction;
//         admittance.Im() = total.Im() * impedance_fraction;
//     }
//     else if (voltage_factor > 0 && !(config & EUC_HEATLOAD)) // Case 2: Not shape-driven, it's a standard ZIP load
//     {
//         // The ZIP components are the source of truth. Calculate the total load from them.
//         total.Re() = power.Re() + current.Re() + admittance.Re();
//         total.Im() = power.Im() + current.Im() + admittance.Im();
//     }
//     // Note: If it's a pure HEATLOAD without a shape, 'total' is assumed to be set by its parent (e.g., a house)

//     // --- Final state updates for the current timestep ---
    
//     // Update peak demand if the current total power is higher
//     if (total.Re() > demand.Re())
//     {
//         demand = total;
//     }

//     // Calculate the new instantaneous heatgain from the current total power
//     if (heatgain_fraction > 0.0)
//     {
//         heatgain = total.Re() * heatgain_fraction * 3412.1416; // Convert kW to Btu/h
//     }

//     // Update the last sync time to the end of the current step
//     t_last = t1;

//     // Return the time of the next required event.
//     // If shape-driven, this is the shape's next update time. Otherwise, no special update is needed.
//     return (shape && shape->type != MT_UNKNOWN) ? shape->t2 : TS_NEVER;
// }


TIMESTAMP enduse::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
    // Energy accumulation...
    if (t_last > TS_ZERO)
    {
        double dt = (double)(t1 - t_last) / 3600.0;
        energy.Re() += total.Re() * dt;
        energy.Im() += total.Im() * dt;
        cumulative_heatgain += heatgain * dt;
        if (dt > 0.0)
            heatgain = 0.0;
    }

    // DEBUG: Check all the conditions
    gl_warning("enduse::postsync '%s': shape=%p, shape->type=%d, shape->load=%g, voltage_factor=%g, power_fraction=%g, current_fraction=%g, impedance_fraction=%g",
        name ? name : "unnamed",
        (void*)shape,
        shape ? (int)shape->type : -1,
        shape ? shape->load : 0.0,
        voltage_factor,
        power_fraction,
        current_fraction,
        impedance_fraction);

    // Load calculation
    if (shape && shape->type != MT_UNKNOWN)
    {
        double P = voltage_factor > 0 ? shape->load:0.0; // * (power_fraction + current_fraction + impedance_fraction) : 0.0;
   
        gl_warning("enduse::postsync '%s': P=%g (from shape->load=%g, voltage_factor=%g)",
            name ? name : "unnamed", P, shape->load, voltage_factor);

        total.Re() = P;
        
        // Apply power factor for reactive component
        if (fabs(power_factor) < 1){
            double Q_multiplier = sqrt(1 / (power_factor * power_factor) - 1);
            total.Im() = (power_factor < 0 ? -1 : 1) * P * Q_multiplier;
            gl_warning("enduse::postsync '%s': Q calculation - power_factor=%g, Q_multiplier=%g, total.Im=%g",
                name ? name : "unnamed", power_factor, Q_multiplier, total.Im());
        }
        else
            total.Im() = 0;

        gl_warning("enduse::postsync '%s': total=(%g,%g), fractions: p=%g, i=%g, z=%g",
            name ? name : "unnamed", 
            total.Re(), total.Im(),
            power_fraction, current_fraction, impedance_fraction);

        // Break down into ZIP components
        power.Re() = total.Re() * power_fraction;
        power.Im() = total.Im() * power_fraction;
        current.Re() = total.Re() * current_fraction;
        current.Im() = total.Im() * current_fraction;
        admittance.Re() = total.Re() * impedance_fraction;
        admittance.Im() = total.Im() * impedance_fraction;

        gl_warning("enduse::postsync '%s': ZIP results - power=(%g,%g), current=(%g,%g), admittance=(%g,%g)",
            name ? name : "unnamed",
            power.Re(), power.Im(),
            current.Re(), current.Im(),
            admittance.Re(), admittance.Im());
    }
    else if (voltage_factor > 0 && !(config & EUC_HEATLOAD))
    {
        total.Re() = power.Re() + current.Re() + admittance.Re();
        total.Im() = power.Im() + current.Im() + admittance.Im();
        gl_warning("enduse::postsync '%s': NON-SHAPE path - computed total=(%g,%g)",
            name ? name : "unnamed", total.Re(), total.Im());
    }

    // Final updates...
    if (total.Re() > demand.Re())
        demand = total;
    
    if (heatgain_fraction > 0.0)
        heatgain = total.Re() * heatgain_fraction * 3412.1416;

    t_last = t1;
    return (shape && shape->type != MT_UNKNOWN) ? shape->t2 : TS_NEVER;
}

typedef struct s_endusesyncdata {
	unsigned int n;
	//pthread_t pt;
	std::thread thread;
	bool ok;
	enduse *e;
	unsigned int ne;
	TIMESTAMP t0;
	unsigned int ran;
} ENDUSESYNCDATA;

// Forward declaration for the thread procedure
// void enduse_syncproc(ENDUSESYNCDATA* data);


// static pthread_cond_t start_ed = PTHREAD_COND_INITIALIZER;
// static pthread_mutex_t startlock_ed = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t done_ed = PTHREAD_COND_INITIALIZER;
// static pthread_mutex_t donelock_ed = PTHREAD_MUTEX_INITIALIZER;
// static TIMESTAMP next_t1_ed, next_t2_ed;
// static unsigned int donecount_ed;
// static unsigned int run = 0;
// 
// clock_t enduse_synctime = 0;
//
//void *enduse_syncproc(void *ptr)
//{
//	ENDUSESYNCDATA *data = (ENDUSESYNCDATA*)ptr;
//	enduse *e;
//	unsigned int n;
//	TIMESTAMP t2;
//
//	// begin processing loop
//	while (data->ok) 
//	{
//		// lock access to start condition
//		pthread_mutex_lock(&startlock_ed);
//
//		// wait for thread start condition
//		while (data->t0==next_t1_ed && data->ran==run) 
//			pthread_cond_wait(&start_ed,&startlock_ed);
//		
//		// unlock access to start count
//		pthread_mutex_unlock(&startlock_ed);
//
//		// process the list for this thread
//		t2 = TS_NEVER;
//		for ( e=data->e, n=0 ; e!=nullptr, n<data->ne ; e=e->next, n++ )
//		{
//			TIMESTAMP t = enduse_sync(e, PC_PRETOPDOWN, next_t1_ed);
//			if (t<t2) t2 = t;
//		}
//
//		// signal completed condition
//		data->t0 = next_t1_ed;
//		data->ran++;
//
//		// lock access to done condition
//		pthread_mutex_lock(&donelock_ed);
//
//		// signal thread is done for now
//		donecount_ed--;
//		if ( t2<next_t2_ed ) next_t2_ed = t2;
//
//		// signal change in done condition
//		pthread_cond_broadcast(&done_ed);
//
//		// unlock access to done count
//		pthread_mutex_unlock(&donelock_ed);
//	}
//	pthread_exit((void*)0);
//	return (void*)0;
//}

static std::condition_variable_any start_ed;         // Replace pthread_cond_t
static unsigned int startlock_ed;                  // Replace pthread_mutex_t
static std::condition_variable_any done_ed;          // Replace pthread_cond_t
static unsigned int donelock_ed;                   // Replace pthread_mutex_t
static TIMESTAMP next_t1_ed, next_t2_ed;
static unsigned int donecount_ed;
static unsigned int run = 0;
clock_t enduse_synctime = 0;

// void enduse_syncproc(ENDUSESYNCDATA* data) {
// 	//ENDUSESYNCDATA* data = static_cast<ENDUSESYNCDATA*>(ptr);
// 	enduse* e;
// 	unsigned int n;
// 	TIMESTAMP t2;

// 	// Begin processing loop
// 	while (data->ok) {
// 		// Lock access to start condition
// 		std::unique_lock<std::shared_mutex> startlock( SharedMutexManager::get_mutex(&startlock_ed));

// 		// Wait for thread start condition
// 		start_ed.wait(startlock, [&]() { return !(data->t0 == next_t1_ed && data->ran == run); });
		

// 		// Unlock access to start count automatically (RAII)

// 		// Process the list for this thread
// 		t2 = TS_NEVER;
// 		for (e = data->e, n = 0; e != nullptr && n < data->ne; e = e->next, n++) {
// 			OBJECT *hdr = object_header(e); 
// 			TIMESTAMP t = enduse_sync(hdr, next_t1_ed, PC_PRETOPDOWN);
// 			if (t < t2) t2 = t;
// 		}

// 		// Signal completion condition
// 		{
// 			std::unique_lock<std::shared_mutex> done_lock(SharedMutexManager::get_mutex(&donelock_ed));
// 			data->t0 = next_t1_ed;
// 			data->ran++;
// 			donecount_ed--;
// 			if (t2 < next_t2_ed) next_t2_ed = t2;

// 			// Notify all other threads that the condition is updated
// 			done_ed.notify_all();
// 		}
// 	}

// 	return;  // Equivalent to pthread_exit (C++ exception-safe threads automatically cleanup)
// }

// Main synchronization function
// TIMESTAMP enduse_syncall(TIMESTAMP t1) {
// 	static unsigned int n_threads_ed = 0;
// 	static std::vector<ENDUSESYNCDATA> thread_ed;

// 	TIMESTAMP t2 = TS_NEVER;
// 	clock_t ts = (clock_t)exec_clock();


// 	// Skip if no enduses exist
// 	if (n_enduses == 0) {
// 		return TS_NEVER;
// 	}

// 	// Initialize threads on first run
// 	if (n_threads_ed == 0) {
// 		enduse* e;
// 		int n_items, en = 0;

// 		output_debug("enduse_syncall setting up for %d enduses", n_enduses);

// 		// Determine thread count
// 		n_threads_ed = global_threadcount;

// 		if (n_threads_ed > 1) {
// 			// Adjust thread count based on workload
// 			if (n_enduses < n_threads_ed * 4) {
// 				n_threads_ed = n_enduses / 4;
// 			}

// 			// Ensure at least one thread
// 			if (n_threads_ed == 0) {
// 				n_threads_ed = 1;
// 			}

// 			// Calculate items per thread
// 			n_items = n_enduses / n_threads_ed;
// 			n_threads_ed = n_enduses / n_items;

// 			// Add extra thread if needed
// 			if (n_threads_ed * n_items < n_enduses) {
// 				n_threads_ed++;
// 			}

// 			output_debug("enduse_syncall is using %d of %d available threads", n_threads_ed, global_threadcount);
// 			output_debug("enduse_syncall is assigning %d enduses per thread", n_items);

// 			// Initialize thread data
// 			thread_ed.resize(n_threads_ed);

// 			// Distribute enduses among threads
// 			for (e = enduse_list; e != nullptr; e = e->next) {
// 				if (en < thread_ed.size() && thread_ed[en].ne == n_items) {
// 					en++;
// 				}

// 				if (en < thread_ed.size() && thread_ed[en].ne == 0) {
// 					thread_ed[en].e = e;
// 				}

// 				if (en < thread_ed.size()) {
// 					thread_ed[en].ne++;
// 				}
// 			}

// 			// Start worker threads
// 			for (unsigned int n = 0; n < n_threads_ed; n++) {
// 				thread_ed[n].ok = true;

// 				// Create and detach thread
// 				thread_ed[n].thread = std::thread(enduse_syncproc, &thread_ed[n]);
// 				thread_ed[n].n = n;
// 				thread_ed[n].thread.detach();
// 			}
// 		}
// 	}

// 	// Single-threaded processing
// 	if (n_threads_ed < 2) {
// 		// Process list directly
// 		for (enduse* e = enduse_list; e != nullptr; e = e->next) {
// 			TIMESTAMP t3 = enduse_sync(e, PC_PRETOPDOWN, t1);
// 			if (t3 < t2) t2 = t3;
// 		}
// 		next_t2_ed = t2;
// 	}
// 	// Multi-threaded processing
// 	else {
// 		// Coordinate thread execution
// 		{
// 			std::unique_lock<std::shared_mutex> done_lock(SharedMutexManager::get_mutex(&donelock_ed));
// 			donecount_ed = n_threads_ed;

// 			{
// 				std::unique_lock<std::shared_mutex> start_lock(SharedMutexManager::get_mutex(&startlock_ed));
// 				next_t1_ed = t1;
// 				next_t2_ed = TS_NEVER;
// 				run++;

// 				start_ed.notify_all();  // Wake up all threads
// 			}

// 			// Wait for all threads to complete
// 			//std::unique_lock<std::shared_mutex> done_wait(done_lock, std::adopt_lock);
// 			//done_ed.wait(done_wait, []() { return donecount_ed == 0; });
// 			// Wait for all threads to complete using the existing lock
// 			done_ed.wait(done_lock, [&]() { return donecount_ed == 0; });

// 			output_debug("passed donecount==0 condition");

// 			// Process results
// 			if (next_t2_ed < t2) t2 = next_t2_ed;
// 		}
// 	}

	

// 	enduse_synctime += (clock_t)exec_clock() - ts;;

// 	return t2;
// }

// TIMESTAMP enduse_syncall(TIMESTAMP t1)
// {
// 	static unsigned int n_threads_ed=0;
// 	static ENDUSESYNCDATA *thread_ed = nullptr;
// 	TIMESTAMP t2 = TS_NEVER;
// 	clock_t ts = (clock_t)exec_clock();
	
// 	// skip enduse_syncall if there's no enduse in the glm
// 	if (n_enduses == 0)
// 		return TS_NEVER;

// 	// number of threads desired
// 	if (n_threads_ed==0)
// 	{
// 		enduse *e;
// 		int n_items, en = 0;

// 		output_debug("enduse_syncall setting up for %d enduses", n_enduses);

// 		// determine needed threads
// 		n_threads_ed = global_threadcount;
// 		if (n_threads_ed>1)
// 		{
// 			unsigned int n;
// 			if (n_enduses<n_threads_ed*4)
// 				n_threads_ed = n_enduses/4;

// 			// only need 1 thread if n_enduses is less than 4
// 			if (n_threads_ed == 0)
// 				n_threads_ed = 1;

// 			// determine enduses per thread
// 			n_items = n_enduses/n_threads_ed;
// 			n_threads_ed = n_enduses/n_items;
// 			if (n_threads_ed*n_items<n_enduses) // not enough slots yet
// 				n_threads_ed++; // add one underused thread

// 			output_debug("enduse_syncall is using %d of %d available threads", n_threads_ed,global_threadcount);
// 			output_debug("enduse_syncall is assigning %d enduses per thread", n_items);

// 			// allocate thread list
// 			thread_ed = (ENDUSESYNCDATA*)malloc(sizeof(ENDUSESYNCDATA)*n_threads_ed);
// 			memset(thread_ed,0,sizeof(ENDUSESYNCDATA)*n_threads_ed);

// 			// assign starting enduse for each thread
// 			for (e=enduse_list; e!=nullptr; e=e->next)
// 			{
// 				if (thread_ed[en].ne==n_items)
// 					en++;
// 				if (thread_ed[en].ne==0)
// 					thread_ed[en].e = e;
// 				thread_ed[en].ne++;
// 			}

// 			// create threads
// 			for (n=0; n<n_threads_ed; n++)
// 			{
// 				thread_ed[n].ok = true;
// 				if (pthread_create(&(thread_ed[n].pt),nullptr,enduse_syncproc,&(thread_ed[n]))!=0)
// 				{
// 					output_fatal("enduse_sync thread creation failed");
// 					thread_ed[n].ok = false;
// 				}
// 				else 
// 					thread_ed[n].n = n;
// 			}
// 		}
// 	}

// 	// no threading required
// 	if (n_threads_ed<2)
// 	{
// 		// process list directly
// 		enduse *e;
// 		for (e=enduse_list; e!=nullptr; e=e->next)
// 		{
// 			TIMESTAMP t3 = enduse_sync(e, PC_PRETOPDOWN, t1);
// 			if (t3<t2) t2 = t3;
// 		}
// 		next_t2_ed = t2;
// 	}
// 	else 
// 	{
// 		// lock access to done count
// 		pthread_mutex_lock(&donelock_ed);

// 		// initialize wait count
// 		donecount_ed = n_threads_ed;

// 		// lock access to start condition
// 		pthread_mutex_lock(&startlock_ed);

// 		// update start condition
// 		next_t1_ed = t1;
// 		next_t2_ed = TS_NEVER;
// 		run++;

// 		// signal all the threads
// 		pthread_cond_broadcast(&start_ed);

// 		// unlock access to start count
// 		pthread_mutex_unlock(&startlock_ed);

// 		// begin wait 
// 		while (donecount_ed>0)
// 			pthread_cond_wait(&done_ed,&donelock_ed);
// 		output_debug("passed donecount==0 condition");

// 		// unclock done count
// 		pthread_mutex_unlock(&donelock_ed);

// 		// process results from all threads
// 		if (next_t2_ed<t2) t2=next_t2_ed;
// 	}

// 	enduse_synctime += (clock_t)exec_clock() - ts;
// 	return t2;

// 	/*enduse *e;
// 	TIMESTAMP t2 = TS_NEVER;
// 	clock_t start = exec_clock();
// 	for (e=enduse_list; e!=nullptr; e=e->next)
// 	{
// 		TIMESTAMP t3 = enduse_sync(e,PC_PRETOPDOWN,t1);
// 		if (t3<t2) t2 = t3;
// 	}
// 	enduse_synctime += exec_clock() - start;
// 	return t2;*/
// }

int convert_from_enduse(char *string,int size,void *data, PROPERTY *prop)
{

    // std::cerr << "convert_from_enduse called!" << std::endl;

/*
	loadshape *shape;
	complex power;
	complex energy;
	complex demand;
	double impedance_fraction;
	double current_fraction;
	double power_fraction;
	double power_factor;
	struct s_enduse *next;
*/
	enduse *e = (enduse*)data;
	int len = 0;
#define OUTPUT_NZ(X) if (e->X!=0) len+=sprintf(string+len,"%s" #X ": %f", len>0?"; ":"", e->X)
#define OUTPUT(X) len+=sprintf(string+len,"%s"#X": %f", len>0?"; ":"", e->X);
	OUTPUT_NZ(impedance_fraction);
	OUTPUT_NZ(current_fraction);
	OUTPUT_NZ(power_fraction);
	OUTPUT(power_factor);
	OUTPUT(power.Re());
	OUTPUT_NZ(power.Im() );
	return len;
}


#include <cstring>
#include <cstdio>

// enduse_publish publishes *subproperties* for a PT_enduse property.
// It assumes class_define_map already published the PT_enduse property itself.
// Example: PT_enduse, "load", PADDR(load) will cause this function to publish
//          "load.power_fraction[pu]", "load.current_fraction[pu]", etc.
int enduse_publish(CLASS *oclass, PROPERTYADDR struct_address, char *prefix)
{
    if (oclass == nullptr)
        return 0;

    gl_warning("enduse_publish called: class=%s, prefix='%s', struct_address=%p",
    oclass->name, prefix ? prefix : "", (void*)struct_address);

    // First, publish the enduse itself as a property
    const char *propname = (prefix == nullptr || strcmp(prefix, "") == 0) ? "load" : prefix;
    PROPERTY *prop = property_malloc(PT_enduse, oclass, const_cast<char*>(propname), struct_address, nullptr);
    prop->description = "the enduse load description";
    prop->flags = 0;
    class_add_property(oclass, prop);

    // We must not use PADDR() here (needs 'this'). Use dummy self + PADDR_C().
    enduse *self = nullptr;

    // Helper to build "prefix.part" or just "part" if prefix is empty/null.
    auto make_name = [](char *dst, size_t dstsz, const char *prefix_, const char *part) {
        if (prefix_ == nullptr || prefix_[0] == '\0') {
            snprintf(dst, dstsz, "%s", part);
        } else {
            snprintf(dst, dstsz, "%s.%s", prefix_, part);
        }
    };

    struct map_entry {
        PROPERTYTYPE type;
        const char *part;         // property part name (without prefix)
        ptrdiff_t off;            // offset within enduse object
        const char *desc;
		const char *unit;     // e.g. "pu", "kVA", "kVAh", "Btu/h", "A"
        // keyword support (only used when type==PT_KEYWORD)
        const char *kw;
        int64 kw_value;
    };

	static const map_entry items[] = {
		{PT_complex, "energy",              (ptrdiff_t)PADDR_C(energy),    "total energy since last reading", "kVAh", nullptr, 0},
		{PT_complex, "power",               (ptrdiff_t)PADDR_C(power),     "constant power (ZIP P)",         "kW",  nullptr, 0},
		{PT_complex, "peak_demand",         (ptrdiff_t)PADDR_C(demand),    "peak power since last reading",   "kVA",  nullptr, 0},

		{PT_double,  "heatgain",            (ptrdiff_t)PADDR_C(heatgain),  "heat transferred to parent",      "Btu/h",nullptr, 0},
		{PT_double,  "cumulative_heatgain", (ptrdiff_t)PADDR_C(cumulative_heatgain),"cumulative heatgain",     "Btu",  nullptr, 0},

		{PT_double,  "heatgain_fraction",   (ptrdiff_t)PADDR_C(heatgain_fraction),"fraction to heat",          "pu",   nullptr, 0},
		{PT_double,  "current_fraction",    (ptrdiff_t)PADDR_C(current_fraction),"const current fraction",     "pu",   nullptr, 0},
		{PT_double,  "impedance_fraction",  (ptrdiff_t)PADDR_C(impedance_fraction),"const impedance fraction", "pu",   nullptr, 0},
		{PT_double,  "power_fraction",      (ptrdiff_t)PADDR_C(power_fraction),"const power fraction",         "pu",   nullptr, 0},

		{PT_double,  "power_factor",        (ptrdiff_t)PADDR_C(power_factor), "power factor",                 nullptr,nullptr, 0},
		{PT_double,  "voltage_factor",      (ptrdiff_t)PADDR_C(voltage_factor),"voltage factor",              "pu",   nullptr, 0},

		{PT_complex, "constant_power",      (ptrdiff_t)PADDR_C(power),      "constant power portion",         "kVA",  nullptr, 0},
		{PT_complex, "constant_current",    (ptrdiff_t)PADDR_C(current),    "constant current portion",       "kVA",  nullptr, 0},
		{PT_complex, "constant_admittance", (ptrdiff_t)PADDR_C(admittance), "constant admittance portion",    "kVA",  nullptr, 0},

		{PT_double,  "breaker_amps",        (ptrdiff_t)PADDR_C(breaker_amps),"rated breaker amps",            "A",    nullptr, 0},

		{PT_set,     "config",              (ptrdiff_t)PADDR_C(config),     "configuration options",          nullptr,nullptr, 0},
		{PT_KEYWORD, nullptr,               0,                              nullptr,                          nullptr,"IS110", (int64)EUC_IS110},
		{PT_KEYWORD, nullptr,               0,                              nullptr,                          nullptr,"IS220", (int64)EUC_IS220},
		{PT_KEYWORD, nullptr,               0,                              nullptr,                          nullptr,"HEATLOAD",(int64)EUC_HEATLOAD},

        {PT_complex, "total",               (ptrdiff_t)PADDR_C(total),     "total power (alias)",             "kVA",  nullptr, 0},  // ADD THIS
        //{PT_complex, "constant_power",      (ptrdiff_t)PADDR_C(power),     "constant power (ZIP P)",          "kW",   nullptr, 0},  // ADD THIS for ZIP power

	};
	

    int published = 0;

    // Track the last real property name/type so PT_KEYWORD can attach to it.
    char last_prop_name[256] = "";
    PROPERTYTYPE last_prop_type = PT_void;

    for (const auto &it : items)
    {
        if (it.type == PT_KEYWORD)
        {
            // Must follow a PT_set or PT_enumeration
            if (last_prop_name[0] == '\0')
            {
                output_error("enduse_publish(%s): PT_KEYWORD without a preceding property", oclass->name);
                return -published;
            }

            if (last_prop_type == PT_set)
            {
                if (!class_define_set_member(oclass, last_prop_name, it.kw, it.kw_value))
                {
                    output_error("enduse_publish(%s): unable to publish set member '%s' for '%s'",
                                 oclass->name, it.kw, last_prop_name);
                    return -published;
                }
            }
            else if (last_prop_type == PT_enumeration)
            {
                // enumeration keyword values are int32 in many places, but your callback uses int64 in some paths;
                // cast down if your class_define_enumeration_member expects enumeration/int32.
                if (!class_define_enumeration_member(oclass, last_prop_name, it.kw, (enumeration)it.kw_value))
                {
                    output_error("enduse_publish(%s): unable to publish enumeration member '%s' for '%s'",
                                 oclass->name, it.kw, last_prop_name);
                    return -published;
                }
            }
            else
            {
                output_error("enduse_publish(%s): PT_KEYWORD after unsupported property type '%s'",
                             oclass->name, class_get_property_typename(last_prop_type));
                return -published;
            }
            continue;
        }

        char fullname[256];
        make_name(fullname, sizeof(fullname), prefix, it.part);

        // addr inside object = struct_address (offset to enduse member) + offset within enduse
        PROPERTYADDR addr = (PROPERTYADDR)((char *)struct_address + it.off);

        PROPERTY *prop = property_malloc(it.type, oclass, fullname, addr, nullptr);
        if (prop == nullptr)
        {
            output_error("enduse_publish(%s): property_malloc failed for '%s'", oclass->name, fullname);
            return -published;
        }

		prop->unit = nullptr;
		if (it.unit && it.unit[0] != '\0') {
			prop->unit = unit_find(it.unit);
			if (prop->unit == nullptr) {
				output_error("enduse_publish(%s): unit '%s' not recognized for '%s'",
							oclass->name, it.unit, fullname);
				return -published;
			}
		}

        prop->description = it.desc;
        prop->flags = 0;

        class_add_property(oclass, prop);
        published++;

        // Update keyword attachment context
        strncpy(last_prop_name, fullname, sizeof(last_prop_name) - 1);
        last_prop_name[sizeof(last_prop_name) - 1] = '\0';
        last_prop_type = it.type;
    }

    return published;
}



// int enduse_publish(CLASS *oclass, PROPERTYADDR struct_address, char *prefix)
// {
// 	// if (prefix != nullptr && strcmp(prefix, "") != 0)
//     // {
//     //     return 1; // Do not publish individual enduse members for sub-objects
//     // }
	
// 	enduse *self=nullptr; // temporary enduse structure used for mapping variables
// 	int result = 0;
//     struct s_map_enduse{
//         PROPERTYTYPE type;
//         const char *name;
//         char *addr = nullptr;
//         const char *description;
//         int64 value = -1;
//         int flags;
//     }*p, prop_list[]={
//             {.type=PT_complex, .name="energy[kVAh]", .addr=(char*)PADDR(energy), .description="the total energy consumed since the last meter reading"},
//             {.type=PT_complex, .name="power[kVA]", .addr=(char*)PADDR(total), .description="the total power consumption of the load"},
//             {.type=PT_complex, .name="peak_demand[kVA]", .addr=(char*)PADDR(demand), .description="the peak power consumption since the last meter reading"},
//             {.type=PT_double, .name="heatgain[Btu/h]", .addr=(char*)PADDR(heatgain), .description="the heat transferred from the enduse to the parent"},
//             {.type=PT_double, .name="cumulative_heatgain[Btu]", .addr=(char*)PADDR(cumulative_heatgain), .description="the cumulative heatgain from the enduse to the parent"},
//             {.type=PT_double, .name="heatgain_fraction[pu]", .addr=(char*)PADDR(heatgain_fraction), .description="the fraction of the heat that goes to the parent"},
//             {.type=PT_double, .name="current_fraction[pu]", .addr=(char*)PADDR(current_fraction),.description="the fraction of total power that is constant current"},
//             {.type=PT_double, .name="impedance_fraction[pu]", .addr=(char*)PADDR(impedance_fraction), .description="the fraction of total power that is constant impedance"},
//             {.type=PT_double, .name="power_fraction[pu]", .addr=(char*)PADDR(power_fraction), .description="the fraction of the total power that is constant power"},
//             {.type=PT_double, .name="power_factor", .addr=(char*)PADDR(power_factor), .description="the power factor of the load"},
//             {.type=PT_complex, .name="constant_power[kVA]", .addr=(char*)PADDR(power), .description="the constant power portion of the total load"},
//             {.type=PT_complex, .name="constant_current[kVA]",    .addr=(char*)PADDR(current), .description="the constant current portion of the total load"},
//             {.type=PT_complex, .name="constant_admittance[kVA]", .addr=(char*)PADDR(admittance), .description="the constant admittance portion of the total load"},
//             {.type=PT_double, .name="voltage_factor[pu]",        .addr=(char*)PADDR(voltage_factor), .description="the voltage change factor"},
//             {.type=PT_double, .name="breaker_amps[A]",           .addr=(char*)PADDR(breaker_amps), .description="the rated breaker amperage"},
//             {.type=PT_set, .name="configuration",                .addr=(char*)PADDR(config), .description="the load configuration options"},
//             {.type=PT_KEYWORD, .name="IS110",                .value=EUC_IS110},
//             {.type=PT_KEYWORD, .name="IS220",                .value=EUC_IS220},
//     }, *last=nullptr;

//     // publish the enduse load itself
// 	PROPERTY *prop = property_malloc(PT_enduse, oclass, const_cast<char *>(strcmp(prefix, "") == 0 ? "load" : prefix), struct_address, nullptr);
// 	prop->description = "the enduse load description";
// 	prop->flags = 0;
// 	class_add_property(oclass,prop);

// 	//if (prefix == nullptr || strcmp(prefix, "") == 0)
// 	//{
// 		for (p = prop_list; p < prop_list + sizeof(prop_list) / sizeof(prop_list[0]); p++)
// 		{
// 			char name[256], lastname[256];

// 			if (prefix == nullptr || strcmp(prefix, "") == 0)
// 			{
// 				strcpy(name, p->name);
// 			}
// 			else
// 			{
// 				strcpy(name,prefix);
// 				strcat(name, ".");
// 				strcat(name, p->name);
// 				// sprintf(name, "%s.%s", prefix, p->name);
// 			}

// 			if (p->type < _PT_LAST)
// 			{
// 				prop = property_malloc(p->type, oclass, name, p->addr + (int64)struct_address, nullptr);
// 				prop->description = p->description;
// 				prop->flags = p->flags;
// 				class_add_property(oclass, prop);
// 				result++;
// 			}
// 			else if (last == nullptr)
// 			{
// 				output_error("PT_KEYWORD not allowed unless it follows another property specification");
// 				/* TROUBLESHOOT
// 					The enduse_publish structure is not defined correctly.  This is an internal error and cannot be corrected by
// 					users.  Contact technical support and report this problem.
// 				 */
// 				return -result;
// 			}
// 			else if (p->type == PT_KEYWORD)
// 			{
// 				switch (last->type)
// 				{
// 				case PT_enumeration:
// 					if (!class_define_enumeration_member(oclass, last->name, p->name, p->type))
// 					{
// 						output_error("unable to publish enumeration member '%s' of enduse '%s'", p->name, last->name);
// 						/* TROUBLESHOOT
// 						The enduse_publish structure is not defined correctly.  This is an internal error and cannot be corrected by
// 						users.  Contact technical support and report this problem.
// 						 */
// 						return -result;
// 					}
// 					break;
// 				case PT_set:
// 					if (!class_define_set_member(oclass, last->name, p->name, p->value))
// 					{
// 						output_error("unable to publish set member '%s' of enduse '%s'", p->name, last->name);
// 						/* TROUBLESHOOT
// 						The enduse_publish structure is not defined correctly.  This is an internal error and cannot be corrected by
// 						users.  Contact technical support and report this problem.
// 						 */
// 						return -result;
// 					}
// 					break;
// 				default:
// 					output_error("PT_KEYWORD not supported after property '%s %s' in enduse_publish", class_get_property_typename(last->type), last->name);
// 					/* TROUBLESHOOT
// 					The enduse_publish structure is not defined correctly.  This is an internal error and cannot be corrected by
// 					users.  Contact technical support and report this problem.
// 					 */
// 					return -result;
// 				}
// 				continue;
// 			}
// 			else
// 			{
// 				output_error("property type '%s' not recognized in enduse_publish", class_get_property_typename(last->type));
// 				/* TROUBLESHOOT
// 					The enduse_publish structure is not defined correctly.  This is an internal error and cannot be corrected by
// 					users.  Contact technical support and report this problem.
// 				*/
// 				return -result;
// 			}

// 			last = p;
// 			strcpy(lastname, name);
// 		} // end for
// 	//} // end if

// 	return result;
// }

int convert_to_enduse(char *string, void *data, PROPERTY *prop)
{
	enduse *e = (enduse*)data;
	char buffer[1024];
	char *token = nullptr;

	/* use structure conversion if opens with { */
	if ( string[0]=='{')
	{
		UNIT *unit = unit_find("kVA");
		PROPERTY eus[] = {
			{nullptr,"total",PT_complex,0,0,PA_PUBLIC,unit,(PROPERTYADDR)((char*)(&e->total)-(char*)e),nullptr,nullptr,nullptr,eus+1},
			{nullptr,"energy",PT_complex,0,0,PA_PUBLIC,unit,(PROPERTYADDR)((char*)(&e->energy)-(char*)e),nullptr,nullptr,nullptr,eus+2},
			{nullptr,"demand",PT_complex,0,0,PA_PUBLIC,unit,(PROPERTYADDR)((char*)(&e->demand)-(char*)e),nullptr,nullptr,nullptr,nullptr},
		};
		return convert_to_struct(string, data, eus);
	}

	/* check string length before copying to buffer */
	if (strlen(string)>sizeof(buffer)-1)
	{
		output_error("convert_to_enduse(string='%-.64s...', ...) input string is too long (max is 1023)",string);
		return 0;
	}
	strcpy(buffer,string);

	/* parse tuples separate by semicolon*/
	while ((token=strtok(token==nullptr?buffer:nullptr,";"))!=nullptr)
	{
		/* colon separate tuple parts */
		char *param = token;
		char *value = strchr(token,':');

		/* isolate param and token and eliminte leading whitespaces */
		while (isspace(*param) || iscntrl(*param)) param++;
		if (value==nullptr)
			value= const_cast<char*>("1");
		else
			*value++ = '\0'; /* separate value from param */
		while (isspace(*value) || iscntrl(*value)) value++;

		// parse params
		if (strcmp(param,"current_fraction")==0)
			e->current_fraction = atof(value);
		else if (strcmp(param,"impedance_fraction")==0)
			e->impedance_fraction = atof(value);
		else if (strcmp(param,"power_fraction")==0)
			e->power_fraction = atof(value);
		else if (strcmp(param,"power_factor")==0)
			e->power_factor = atof(value);
		else if ( strcmp(param,"power.r")==0 )
			e->power.Re() = atof(value);
		else if ( strcmp(param,"power.i")==0 )
			e->power.Im() = atof(value);
		else if (strcmp(param,"loadshape")==0)
		{
			PROPERTY *pref = class_find_property(prop->oclass,value);
			if (pref==nullptr)
			{
				output_warning("convert_to_enduse(string='%-.64s...', ...) loadshape '%s' not found in class '%s'",string,value,prop->oclass->name);
				return 0;
			}
			e->shape = (loadshape*)((char*)e - (int64)(prop->addr) + (int64)(pref->addr));
		}
		else
		{
			output_error("convert_to_enduse(string='%-.64s...', ...) parameter '%s' is not valid",string,param);
			return 0;
		}
	}

	/* reinitialize the loadshape */
	// if (enduse_init((enduse*)data))
	// 	return 0;

	/* everything converted ok */
	return 1;
}

int enduse_test(void)
{
	int failed = 0;
	int ok = 0;
	int errorcount = 0;

	/* tests */
	struct s_test {
		const char *name;
	} *p, test[] = {
		"TODO",
	};

	output_test("\nBEGIN: enduse tests");
	for (p=test;p<test+sizeof(test)/sizeof(test[0]);p++)
	{
	}

	/* report results */
	if (failed)
	{
		output_error("endusetest: %d enduse tests failed--see test.txt for more information",failed);
		output_test("!!! %d enduse tests failed, %d errors found",failed,errorcount);
	}
	else
	{
		output_verbose("%d enduse tests completed with no errors--see test.txt for details",ok);
		output_test("endusetest: %d schedule tests completed, %d errors found",ok,errorcount);
	}
	output_test("END: enduse tests");
	return failed;
}


extern "C" void enduse_syncproc(ENDUSESYNCDATA* data) {
    TIMESTAMP t2;

    while (data->ok) {
        // Wait for start signal
        std::unique_lock<std::shared_mutex> startlock(SharedMutexManager::get_mutex(&startlock_ed));
        start_ed.wait(startlock, [&]() { return !(data->t0 == next_t1_ed && data->ran == run); });

        // Process the assigned list of objects
        t2 = TS_NEVER;
        enduse* e = data->e;
        OBJECT* hdr = object_header(e);

        // This loop iterates through the core's object list, but only processes 'ne' enduse objects
        for (unsigned int n = 0; n < data->ne && hdr != nullptr; ) {
            if (hdr->oclass == enduse::oclass) {
                TIMESTAMP t = enduse_sync(hdr, next_t1_ed,PC_PRETOPDOWN);
                if (t < t2) t2 = t;
                n++; // Increment count only when we process an enduse
            }
            hdr = hdr->next; // Move to the next object in the global list
        }

        // Signal completion
        {
            std::unique_lock<std::shared_mutex> done_lock(SharedMutexManager::get_mutex(&donelock_ed));
            data->t0 = next_t1_ed;
            data->ran++;
            donecount_ed--;
            if (t2 < next_t2_ed) next_t2_ed = t2;
            done_ed.notify_all();
        }
    }
}

extern "C" TIMESTAMP enduse_syncall(TIMESTAMP t1) {
    static unsigned int n_threads_ed = 0;
    static std::vector<ENDUSESYNCDATA> thread_ed;
    static std::vector<std::thread> threads;

    TIMESTAMP t2 = TS_NEVER;
    clock_t ts = (clock_t)exec_clock();

    // --- Modern iteration to count objects ---
    unsigned int n_enduses = 0;
    if (enduse::oclass != nullptr) {
        // We must iterate all objects and check their class
        for (OBJECT *obj = object_get_first(); obj != nullptr; obj = obj->next) {
            if (obj->oclass == enduse::oclass) {
                n_enduses++;
            }
        }
    }

    if (n_enduses == 0) {
        return TS_NEVER; // No work to do
    }

    // --- Thread initialization (first run only) ---
    if (n_threads_ed == 0) {
        n_threads_ed = global_threadcount;
        if (n_threads_ed > n_enduses) n_threads_ed = n_enduses;
        if (n_threads_ed == 0) n_threads_ed = 1;

        output_debug("enduse_syncall is using %d threads for %d enduses", n_threads_ed, n_enduses);

        if (n_threads_ed > 1) {
            thread_ed.resize(n_threads_ed);

            // Distribute enduse objects among threads
            unsigned int items_per_thread = n_enduses / n_threads_ed;
            unsigned int remainder = n_enduses % n_threads_ed;
            OBJECT *current_obj = object_get_first();
            
            for (unsigned int i = 0; i < n_threads_ed; ++i) {
                // Find the first enduse object for this thread
                while(current_obj != nullptr && current_obj->oclass != enduse::oclass) {
                    current_obj = current_obj->next;
                }
                if (current_obj == nullptr) break; // Should not happen

                thread_ed[i].e = object_data<enduse>(current_obj);
                thread_ed[i].ne = items_per_thread + (i < remainder ? 1 : 0);

                // Advance object pointer for the next thread
                for (unsigned int j = 0; j < thread_ed[i].ne && current_obj != nullptr; ) {
                    current_obj = current_obj->next;
                    // Only count actual enduse objects
                    if (current_obj != nullptr && current_obj->oclass == enduse::oclass) {
                        j++;
                    }
                }
            }

            // Start worker threads using std::thread
            for (unsigned int n = 0; n < n_threads_ed; n++) {
                thread_ed[n].ok = true;
                thread_ed[n].n = n;
                std::thread t(enduse_syncproc, &thread_ed[n]);
                t.detach();
            }
        }
    }
    
    // --- Synchronization Logic ---
    if (n_threads_ed < 2) { // Single-threaded case
        for (OBJECT *obj = object_get_first(); obj != nullptr; obj = obj->next) {
            if (obj->oclass == enduse::oclass) {
                TIMESTAMP t3 = enduse_sync(obj, t1, PC_PRETOPDOWN);
                if (t3 < t2) t2 = t3;
            }
        }
    } else { // Multi-threaded case
        std::unique_lock<std::shared_mutex> done_lock(SharedMutexManager::get_mutex(&donelock_ed));
        donecount_ed = n_threads_ed;

        {
            std::unique_lock<std::shared_mutex> start_lock(SharedMutexManager::get_mutex(&startlock_ed));
            next_t1_ed = t1;
            next_t2_ed = TS_NEVER;
            run++;
            start_ed.notify_all();
        }

        done_ed.wait(done_lock, [&]() { return donecount_ed == 0; });
        t2 = next_t2_ed;
    }

    enduse_synctime += (clock_t)exec_clock() - ts;
    return t2;
}



