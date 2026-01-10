/* double_assert

   Very simple test that compares double values to any corresponding double value.  If the test
   fails at any time, it t.rows() a 'zero' to the commit function and breaks the simulator out with
   a failure code.
*/


#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <gld_complex.h>

#include <algorithm>
#include <string>


#include "double_assert.h"

EXPORT_CREATE(double_assert);
EXPORT_INIT(double_assert);
EXPORT_COMMIT(double_assert);
//EXPORT_NOTIFY(double_assert);

CLASS *double_assert::oclass = nullptr;
// double_assert *double_assert::defaults = nullptr;
// static double_assert defaults_storage; // POD storage for defaults
// double_assert *double_assert::defaults = &defaults_storage;



// Helper: treat aggregates under "record.*" as uninitialized when exactly zero
static inline bool looks_uninitialized(const std::string& target, double x)
{
    if (!std::isfinite(x)) return true;        // NaN/Inf => not ready
    if (x == 0.0) {
        if (target.rfind("record.", 0) == 0)   // starts_with("record.")
            return true;
    }
    return false;
 }


// --- Heuristic: decide SNAPSHOT vs CONTINUOUS without any GLM changes ---
static bool model_is_continuous_like()
{
    // 1) Long duration? Treat as continuous (>= 1 hour)
    char startbuf[64] = {0}, stopbuf[64] = {0};
    gl_global_getvar("starttime", startbuf, sizeof(startbuf));
    gl_global_getvar("stoptime",  stopbuf,  sizeof(stopbuf));
    TIMESTAMP t_start = gl_parsetime(startbuf);
    TIMESTAMP t_stop  = gl_parsetime(stopbuf);
    bool long_run = (t_start > 0 && t_stop > t_start && (t_stop - t_start) >= 3600);

    // 2) Presence of powerflow links/devices => continuous
    auto has_any = [] (const char* cls) -> bool {
        FINDLIST* fl = gl_find_objects(FL_NEW, FT_CLASS, SAME, cls, FT_END);
        bool ok = (fl && fl->hit_count > 0);
        if (fl) gl_free((void**)&fl);
        return ok;
    };
    bool has_links   = has_any("overhead_line") || has_any("underground_line") || has_any("triplex_line");
    bool has_devices = has_any("regulator") || has_any("switch") || has_any("recloser")
                    || has_any("fuse")      || has_any("capacitor");

    return long_run || has_links || has_devices;
}

// Safe helper for future scheduling
static inline TIMESTAMP schedule_future(TIMESTAMP now, TIMESTAMP next)
{
    return (next <= now) ? (now + 1) : next;
}



double_assert::double_assert(MODULE *module)
{
	// defaults = this;
	status = ASSERT_TRUE;
	within = 0.0;
	within_mode = IN_ABS;
	value = 0.0;
	once = ONCE_FALSE;
	once_value = 0;
	target.erase();
	skip_uninitialized = true;
	
	uninit_retries       = 0;
    uninit_max_retries   = 60;  // default: up to 60 tries
    uninit_retry_step    = 60;  // default: retry every 60 seconds

	
	if (oclass == nullptr)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		//oclass = gl_register_class(module, "double_assert", sizeof(struct double_assert), PC_AUTOLOCK | PC_OBSERVER);
		oclass = gld_class::create(module, "double_assert", sizeof(double_assert), PC_AUTOLOCK | PC_OBSERVER );

		if (oclass == nullptr){
			// throw "unable to register class double_assert";
		    gl_error("unable to register class double_assert");
            return; // Exit cleanly on failure
		}
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
								PT_enumeration, "within_mode", get_within_mode_offset(), PT_DESCRIPTION, "Method of applying tolerance",
								PT_KEYWORD, "IN_ABS", (enumeration)IN_ABS,
								PT_KEYWORD, "IN_RATIO", (enumeration)IN_RATIO,
								PT_double, "value", get_value_offset(), PT_DESCRIPTION, "Value to assert",
								PT_double, "once_value", get_once_value_offset(), PT_DESCRIPTION, "Value for a single assert check",
								PT_double, "within", get_within_offset(), PT_DESCRIPTION, "Tolerance for a successful assert",
								PT_char1024, "target", get_target_offset(), PT_DESCRIPTION, "Property to perform the assert upon",
								PT_timestamp, "in",  get_ts_in_offset(),  PT_DESCRIPTION, "Earliest time to evaluate",
								PT_timestamp, "out", get_ts_out_offset(), PT_DESCRIPTION, "Latest time to evaluate",
                                PT_bool, "skip_uninitialized", get_skip_uninitialized_offset(),PT_DESCRIPTION, "Skip and re-check if aggregate looks uninitialized (e.g., record.* == 0)",
                                PT_int32,     "uninit_retry_step", get_uninit_retry_step_offset(),PT_DESCRIPTION, "Retry cadence in seconds when skipping uninitialized aggregates",
                                PT_int32,     "uninit_max_retries",  get_uninit_max_retries_offset(),PT_DESCRIPTION, "Maximum number of retries when aggregate looks uninitialized",
								nullptr) < 1)
		{
			char msg[256];
			sprintf(msg, "unable to publish properties in %s", __FILE__);
			throw msg;
		}

		
	}

	
}

/* Object creation is called once for each object that is created by the core */
int double_assert::create(void)
{
	
	status = ASSERT_TRUE;
    within = 0.0;
    within_mode = IN_ABS;
    value = 0.0;
    once = ONCE_FALSE;
    once_value = 0;
    target.erase();
	ts_in  = TS_INVALID;
	ts_out = TS_NEVER;

	skip_uninitialized = true;
    tried_uninit_once  = false;
    uninit_retries       = 0;
    uninit_max_retries   = 60;
    uninit_retry_step    = 60;



	gl_output("double_assert defaults: status=%d value=%g within=%g within_mode=%d once=%d target='%s' once_value=%g",
			  static_cast<int>(status),
			  value, within,
			  static_cast<int>(within_mode),
			  static_cast<int>(once),
			  target.get_string(), once_value);

	return 1; /* return 1 on success, 0 on failure */
}

int double_assert::init(OBJECT *parent)
{
	
	pDouble = nullptr;

    if (within <= 0.0) {
        gl_warning("double_assert: a non-positive value has been specified for 'within'");
    }

	return 1; // Success

}

TIMESTAMP double_assert::commit(TIMESTAMP t1, TIMESTAMP t2)
{

	// Time-gating before evaluation
	const TIMESTAMP now = gl_globalclock;
	// if ((ts_in != TS_INVALID && now < ts_in) || (ts_out != TS_NEVER && now > ts_out)) {
	// 	// Outside the window: do nothing
	// 	return TS_NEVER;
	// }


	if (((ts_in  != TS_INVALID) && (now < ts_in)) ||
	      ((ts_out != TS_NEVER)   && (now > ts_out))) {

	    // Outside the evaluation window – skip silently
		return TS_NEVER;
	}

	
    // --- azy property resolution ---
    // If pDouble is null, this is our first time running. Link the property now.
    if (pDouble == nullptr)
    {
        OBJECT *parent = get_parent()->my();
        if (parent == nullptr)
        {
            // This should not happen if the model is structured correctly
			gl_error("double_assert:%d: object requires a 'parent' to be defined. The 'parent' property is missing.", get_id());
            return TS_INVALID;
        }

        // 1. Find the PROPERTY structure
        PROPERTY *pTarget = gl_get_property(parent, get_target().c_str());
        if (pTarget == nullptr) {
            gl_error("double_assert:%d: target property '%s' not found in parent '%s'", get_id(), get_target().c_str(), parent->name);
            return TS_INVALID; // Stop simulation
        }

        // 2. Verify property type
        if (pTarget->ptype != PT_double) {
            gl_error("double_assert:%d: target property '%s' is not of type double", get_id(), get_target().c_str());
            return TS_INVALID; // Stop simulation
        }

        // 3. Get and cache the direct memory address
        // pDouble = (double*)gl_get_addr(parent, get_target().c_str());
        // if (pDouble == nullptr) {
        //     gl_error("double_assert:%d: unable to get address of target property '%s'", get_id(), get_target().c_str());
        //     return TS_INVALID; // Stop simulation
        // }


		// Re-fetch the value every time; avoid cached pDouble
		pDouble = (double*)gl_get_double_by_name(parent, get_target().c_str());
		if (pDouble == nullptr) {
			gl_error("double_assert:%d: target '%s' not found in parent '%s'", get_id(), get_target().c_str(), parent->name);
			return TS_INVALID;
		}


    }


	// handle once mode
	if (once == ONCE_TRUE)
	{
		once_value = value;
		once = ONCE_DONE;
	}
	else if (once == ONCE_DONE)
	{
		if (once_value == value)
		{
			gl_verbose("Assert skipped with ONCE logic");
			return TS_NEVER;
		}
		else
		{
			once_value = value;
		}
	}


	// get the within range
	double range = 0.0;
	if (within_mode == IN_RATIO)
	{
		// range = value * within;
		// if (range < 0.001)
		// { // minimum bounds
		// 	range = 0.001;
		// }
	}
	else if (within_mode == IN_ABS)
	{
		range = within;
	}

    // --- Always evaluate ---
    double x = *pDouble;

	
    // Smarter defer: aggregate fields (record.*) without explicit windows
    // may read as 0.0 at starttime. Retry on a bounded cadence until initialized.
    if (skip_uninitialized
        && ts_in == TS_INVALID && ts_out == TS_NEVER
        && looks_uninitialized(get_target(), x))
    {
        // Get stoptime (to avoid scheduling beyond it)
        char stopbuf[64] = {0};
        gl_global_getvar("stoptime", stopbuf, sizeof(stopbuf));
        TIMESTAMP t_stop = gl_parsetime(stopbuf);

        // Decide next retry time
        TIMESTAMP next = gl_globalclock + uninit_retry_step;
        if (t_stop > 0 && next >= t_stop) {
            // If we're too close to stoptime, just stop rescheduling and proceed to evaluation
            gl_verbose("double_assert(%s): '%s' near stoptime; proceeding despite uninitialized (%g)",
                       get_name(), get_target().c_str(), x);
        } else if (uninit_retries < uninit_max_retries) {
            uninit_retries++;
            gl_verbose("double_assert(%s): deferring '%s' (current=%g), retry %u/%u in %u s",
                       get_name(), get_target().c_str(), x,
                       uninit_retries, uninit_max_retries, (unsigned)uninit_retry_step);
            return schedule_future(gl_globalclock, next);
        } else {
            gl_verbose("double_assert(%s): '%s' still uninitialized after %u retries (%g), proceeding",
                       get_name(), get_target().c_str(), uninit_max_retries, x);
        }
    }



    if (!std::isfinite(x)) {
        // Skip, but do not fail due to non-finite read
        return TS_NEVER;
    }
 
     if (status == ASSERT_TRUE)
     {
        double m = std::fabs(x - value);
        if (std::isnan(m) || m > range)
         {
             gl_error("Assert failed on %s: %s %g not within %f of given value %g",
                      get_parent()->get_name(), get_target().c_str(), x, range, value);
            return TS_INVALID;
         }
        //  gl_verbose("Assert passed on %s", get_parent()->get_name());
         return TS_NEVER;
     }
     else if (status == ASSERT_FALSE)
     {
        double m = std::fabs(x - value);
        if (std::isnan(m) || m < range)
         {
             gl_error("Assert failed on %s: %s %g is within %f of given value %g",
                      get_parent()->get_name(), get_target().c_str(), x, range, value);
            return TS_INVALID;
         }
        //  gl_verbose("Assert passed on %s", get_parent()->get_name());
         return TS_NEVER;
     }
     else
     {
         gl_verbose("Assert test is not being run on %s", get_parent()->get_name());
         return TS_NEVER;
     }

}

int double_assert::postnotify(PROPERTY *prop, char *value)
{
	if (once == ONCE_DONE && strcmp(prop->name, "value") == 0)
	{
		once = ONCE_TRUE;
	}
	return 1;
}

// EXPORT for object-level call (as opposed to module-level)
EXPORT SIMULATIONMODE update_double_assert(OBJECT *obj, TIMESTAMP t0, unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{
	char buff[128];
	char dateformat[16] = "";
	char error_output_buff[2028];
	char datebuff[128];
	double_assert *da = object_data<double_assert>(obj);
	DATETIME delta_dt_val;
	double del_clock;
	TIMESTAMP del_clock_int;
	int del_microseconds;
	double *x;

	if (da->get_once() == da->ONCE_TRUE)
	{
		da->set_once_value(da->get_value());
		da->set_once(da->ONCE_DONE);
	}
	else if (da->get_once() == da->ONCE_DONE)
	{
		if (da->get_once_value() == da->get_value())
		{
			gl_verbose("Assert skipped with ONCE logic");
			return SM_EVENT;
		}
		else
		{
			da->set_once_value(da->get_value());
		}
	}

	// get the within range
	double range = 0.0;
	if (da->get_within_mode() == da->IN_RATIO)
	{
		range = da->get_value() * da->get_within();

		// if ( range<0.001 ) //minimum bounds removed since many deltamode items are small
		//{	// minimum bounds
		//	range = 0.001;
		// }
	}
	else if (da->get_within_mode() == da->IN_ABS)
	{
		range = da->get_within();
	}

	// Iteration checker - assert only valid on the first timestep
	if (iteration_count_val == 0)
	{
		// Skip first timestep of any delta iteration -- nature of delta means it really isn't checking the right one
		if (delta_time >= dt)
		{
			// Get value
			x = (double *)gl_get_double_by_name(obj->parent, da->get_target().c_str());

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
			else if (da->get_status() == da->ASSERT_TRUE)
			{
				double m = fabs(*x - da->get_value());
				if (_isnan(m) || m > range)
				{
					// Calculate time
					if (delta_time >= dt) // After first iteration
						del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
					else // First second different, don't back out
						del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

					del_clock_int = (TIMESTAMP)del_clock;									  /* Whole seconds - update from global clock because we could be in delta for over 1 second */
					del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

					// Convert out
					gl_localtime(del_clock_int, &delta_dt_val);

					// Determine output format
					gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

					// Output date appropriately
					if (strcmp(dateformat, "ISO") == 0)
						sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
					else if (strcmp(dateformat, "US") == 0)
						sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
					else if (strcmp(dateformat, "EURO") == 0)
						sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
					else
						sprintf(datebuff, "ERROR    %.09lf : ", del_clock);

					// Actual error part
					sprintf(error_output_buff, "Assert failed on %s - %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), *x, da->get_within(), da->get_value());

					// Send it out
					gl_output("%s%s", datebuff, error_output_buff);

					return SM_ERROR;
				}
				// gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
				return SM_EVENT;
			}
			else if (da->get_status() == da->ASSERT_FALSE)
			{
				double m = fabs(*x - da->get_value());
				if (_isnan(m) || m < range)
				{
					// Calculate time
					if (delta_time >= dt) // After first iteration
						del_clock = (double)t0 + (double)(delta_time - dt) / (double)DT_SECOND;
					else // First second different, don't back out
						del_clock = (double)t0 + (double)(delta_time) / (double)DT_SECOND;

					del_clock_int = (TIMESTAMP)del_clock;									  /* Whole seconds - update from global clock because we could be in delta for over 1 second */
					del_microseconds = (int)((del_clock - (int)(del_clock)) * 1000000 + 0.5); /* microseconds roll-over - biased upward (by 0.5) */

					// Convert out
					gl_localtime(del_clock_int, &delta_dt_val);

					// Determine output format
					gl_global_getvar("dateformat", dateformat, sizeof(dateformat));

					// Output date appropriately
					if (strcmp(dateformat, "ISO") == 0)
						sprintf(datebuff, "ERROR    [%04d-%02d-%02d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.year, delta_dt_val.month, delta_dt_val.day, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
					else if (strcmp(dateformat, "US") == 0)
						sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.month, delta_dt_val.day, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
					else if (strcmp(dateformat, "EURO") == 0)
						sprintf(datebuff, "ERROR    [%02d-%02d-%04d %02d:%02d:%02d.%.06d %s] : ", delta_dt_val.day, delta_dt_val.month, delta_dt_val.year, delta_dt_val.hour, delta_dt_val.minute, delta_dt_val.second, del_microseconds, delta_dt_val.tz);
					else
						sprintf(datebuff, "ERROR    %.09lf : ", del_clock);

					// Actual error part
					sprintf(error_output_buff, "Assert failed on %s - %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), *x, da->get_within(), da->get_value());

					// Send it out
					gl_output("%s%s", datebuff, error_output_buff);

					return SM_ERROR;
				}
				// gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
				return SM_EVENT;
			}
			else
			{
				gl_verbose("Assert test is not being run on %s", gl_name(obj->parent, buff, 64));
				return SM_EVENT;
			}
		}
		else // First pass, just proceed
			return SM_EVENT;
	}
	else // Iteration, so don't care
		return SM_EVENT;
}
