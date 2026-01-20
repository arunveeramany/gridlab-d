/* enum_assert

   Very simple test that compares either integers or can be used to compare enumerated values
   to their corresponding integer values.  If the test fails at any time, it t.rows() a 'zero' to
   the commit function and breaks the simulator out with a failure code.
*/

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gld_complex.h>

#include "enum_assert.h"
#include "object.h"

// Required module version info to match the core
// Corrected, non-conflicting version variables
EXPORT int gld_major = 5;
EXPORT int gld_minor = 3;


EXPORT_CREATE(enum_assert);
EXPORT_INIT(enum_assert);
EXPORT_COMMIT(enum_assert);

CLASS *enum_assert::oclass = nullptr;
enum_assert *enum_assert::defaults = nullptr;

extern "C" CALLBACKS *callback;

// EXPORT CLASS* init(CALLBACKS *fntable, MODULE *mod, int argc, char *argv[])
// {
//     callback = fntable;
// 	std::cerr << "Received callback table in init_enum_assert at address: " << (void*)callback << std::endl;

// 	if (!callback) {
//         std::cerr << "FATAL: init_enum_assert received null callback table" << std::endl;
//         return 0;
//     }
//     std::cerr << "Callback initialized at address: " << (void*)callback << std::endl;
// 	std::cerr << "properties.get_property callback value: " << (void*)callback->properties.get_property << std::endl;

// 	if (!callback->properties.get_property) {
//         std::cerr << "FATAL: properties.get_property callback is null" << std::endl;
//         return 0;
//     }
// 	std::cerr << "properties.get_property callback initialized at address: " << (void*)callback->properties.get_property << std::endl;

//     new enum_assert(mod); // Instantiate the class to trigger registration
//     // return 1;
// 	return enum_assert::oclass;
// }



// --- Heuristic: decide SNAPSHOT vs CONTINUOUS without GLM changes ---
static bool model_is_continuous_like()
{
    // 1) Long duration? Treat as continuous.
    char startbuf[64] = {0}, stopbuf[64] = {0};
    gl_global_getvar("starttime", startbuf, sizeof(startbuf));
    gl_global_getvar("stoptime",  stopbuf,  sizeof(stopbuf));
    TIMESTAMP t_start = gl_parsetime(startbuf);
    TIMESTAMP t_stop  = gl_parsetime(stopbuf);
    bool long_run = (t_start > 0 && t_stop > t_start && (t_stop - t_start) >= 3600); // >= 1 hour

    // 2) Presence of network/controls? Treat as continuous.
    auto has_any = [] (const char* cls) -> bool {
        FINDLIST* fl = gl_find_objects(FL_NEW, FT_CLASS, SAME, cls, FT_END);
        bool ok = (fl && fl->hit_count > 0);
        if (fl) gl_free((void**)&fl);
        return ok;
    };

    bool has_links =
        has_any("overhead_line") || has_any("underground_line") || has_any("triplex_line");
    bool has_devices =
        has_any("regulator") || has_any("switch") || has_any("recloser") ||
        has_any("fuse") || has_any("capacitor");

    return long_run || has_links || has_devices;
}



enum_assert::enum_assert(MODULE *module): gld_object() 
{
	if (oclass == nullptr)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		oclass = gld_class::create(module, "enum_assert", sizeof(enum_assert), PC_AUTOLOCK | PC_OBSERVER);
		if (oclass == nullptr)
			throw "unable to register class enum_assert";
		else
			oclass->trl = TRL_PROVEN;

		if (gl_publish_variable(oclass,
								// TO DO:  publish your variables here
								PT_enumeration, "status", get_status_offset(), PT_DESCRIPTION, "Conditions for the assert checks",
								PT_KEYWORD, "ASSERT_TRUE", (enumeration)ASSERT_TRUE,
								PT_KEYWORD, "ASSERT_FALSE", (enumeration)ASSERT_FALSE,
								PT_KEYWORD, "ASSERT_NONE", (enumeration)ASSERT_NONE,
								PT_char1024, "value", get_value_text_offset(), PT_DESCRIPTION, "Value to assert",
								PT_int32,    "value_code", get_value_code_offset(), // parsed numeric target
								PT_char1024, "target", get_target_offset(), PT_DESCRIPTION, "Property to perform the assert upon",
								nullptr) < 1)
		{
			char msg[256];
			sprintf(msg, "unable to publish properties in %s", __FILE__);
			throw msg;
		}

		defaults = this;
		status = ASSERT_TRUE;
		value_code = 0;
		strcpy(value_text, ""); 
	}
}

/* Object creation is called once for each object that is created by the core */
int enum_assert::create(void)
{
	//memcpy(this, defaults, sizeof(*this));

	memset(target, 0, sizeof(target));
    memset(value_text, 0, sizeof(value_text));
    status = ASSERT_TRUE;
    value_code = 0;

	return 1; /* return 1 on success, 0 on failure */
}


static bool parse_int(const char *s, int *out)
{
    if (!s || !*s) return false;
    char *end = nullptr;
    long v = strtol(s, &end, 10);
    if (end && *end == '\0') { *out = (int)v; return true; }
    return false;
}

// Map string enum names to integer codes
// Adjust names/codes to match your GFA_trip_method enumeration
static bool map_enum_name_to_code(const std::string &name, int *code)
{
    // normalize
    auto norm = name;
    // strip spaces
    norm.erase(remove_if(norm.begin(), norm.end(), ::isspace), norm.end());
    // upper-case
    std::transform(norm.begin(), norm.end(), norm.begin(), ::toupper);

    if      (norm == "NONE")            { *code = 0; return true; }
    else if (norm == "UNDER_FREQUENCY") { *code = 1; return true; }
    else if (norm == "OVER_FREQUENCY")  { *code = 2; return true; }
    else if (norm == "UNDER_VOLTAGE")   { *code = 3; return true; }
    else if (norm == "OVER_VOLTAGE")    { *code = 4; return true; }

    return false; // unknown
}

// EXPORT int init_enum_assert(OBJECT *obj, OBJECT *parent)
// {
//     try {
//         if (obj != nullptr) {
//             enum_assert *my = object_data<enum_assert>(obj);
//             return my->init(parent);
//         }
//         return 0;
//     }
//     catch (const char *msg) {
//         gl_error("init_enum_assert: %s", msg);
//         return 0;
//     }
//     catch (...) {
//         gl_error("init_enum_assert: unhandled exception");
//         return 0;
//     }
// }



int enum_assert::init(OBJECT *parent)
{
    try {
        std::cerr << "enum_assert::init() starting" << std::endl;
        
        OBJECT *obj = my();
        if (!obj) {
            gl_error("enum_assert::init: my() returned NULL");
            return 0;
        }
        std::cerr << "enum_assert::init: my() = " << (void*)obj << std::endl;
        
        gl_verbose("  sizeof(gld_object) = %zu", sizeof(gld_object));
        gl_verbose("  sizeof(OBJECT) = %zu", sizeof(OBJECT));
        gl_verbose("  offsetof(enum_assert, target) = %zu", offsetof(enum_assert, target));
        gl_verbose("  obj+1 address = %p", (void*)((char*)obj + sizeof(OBJECT)));
        gl_verbose("  this address = %p", (void*)this);
        gl_verbose("  target member address = %p", (void*)target);
        
        // Calculate difference using char* for byte-level math
        char* data_start = (char*)obj + sizeof(OBJECT);
        gl_verbose("  difference (this - data_start) = %td", (char*)this - data_start);
        gl_verbose("  difference (target - data_start) = %td", (char*)target - data_start);
        
        // Try reading from where GridLAB-D wrote it
        size_t published_offset = get_target_offset();
        gl_verbose("  published offset = %zu", published_offset);
        gl_verbose("  string at data_start + offset: '%s'", data_start + published_offset);
        gl_verbose("  string at target member: '%s'", target);

        std::cerr << "enum_assert::init: checking parent" << std::endl;
        gld_object* pobj = get_parent();
        if (!pobj) {
            gl_error("enum_assert: parent is null");
            return 0;
        }
        std::cerr << "enum_assert::init: parent OK" << std::endl;

        std::cerr << "enum_assert::init: getting target" << std::endl;
        const char* prop_name = target;  // Use raw member directly!
        std::cerr << "enum_assert::init: target = '" << (prop_name ? prop_name : "(null)") << "'" << std::endl;
        
        if (!prop_name || *prop_name == '\0') {
            gl_error("enum_assert: target property name is empty");
            return 0;
        }

        std::cerr << "enum_assert::init: resolving property on parent" << std::endl;
        // Resolve target property on parent
        gld_property target_prop(pobj->my(), const_cast<char*>(prop_name));    
        if (!target_prop.is_valid()) {
            gl_error("enum_assert: target property '%s' invalid on '%s'",
                     prop_name, 
                     pobj->get_name() ? pobj->get_name() : "(no-name)");
            return 0;
        }

        std::cerr << "enum_assert::init: parsing value text" << std::endl;
        // Parse expected "value" (text) into value_code
        int tmp = 0;
        const char* vt = value_text;  // Use raw member directly!
        std::cerr << "enum_assert::init: value_text = '" << (vt ? vt : "(null)") << "'" << std::endl;
        
        if (!vt || !*vt) {
            gl_error("enum_assert: expected value text is empty");
            return 0;
        }
        
        if (std::isdigit(static_cast<unsigned char>(vt[0])) || vt[0] == '-') {
            tmp = std::atoi(vt);
        } else {
            if (!map_enum_name_to_code(vt, &tmp)) {
                gl_error("enum_assert: cannot parse value '%s' as numeric or known enum name", vt);
                return 0;
            }
        }
        set_value_code(tmp);
        
        std::cerr << "enum_assert::init: completed successfully" << std::endl;
        return 1;
    }
    catch (const char* msg) {
        gl_error("enum_assert::init exception: %s", msg ? msg : "(null)");
        return 0;
    }
    catch (const std::exception& e) {
        gl_error("enum_assert::init std::exception: %s", e.what());
        return 0;
    }
    catch (...) {
        gl_error("enum_assert::init: unknown exception");
        return 0;
    }
}

// TIMESTAMP enum_assert::commit(TIMESTAMP t0, TIMESTAMP t1)
// {
//     gld_object* pobj = get_parent();
//     if (!pobj) throw "enum_assert: parent is null";

//     // --- get actual ---
//     const char* prop_name = get_target().c_str();
//     gld_property actual_prop(pobj->my(), const_cast<char*>(prop_name));
//     if (!actual_prop.is_valid()) { /* throw or log */ }

//     char actual_text[64] = {0};
//     actual_prop.to_string(actual_text, sizeof(actual_text)); // keyword or numeric text
// 	int actual_code = 0;
// 	if (std::isdigit(static_cast<unsigned char>(actual_text[0])) || actual_text[0] == '-') {
// 		actual_code = std::atoi(actual_text);
// 	} else {
// 		// map keyword -> code using existing helper
// 		if (!map_enum_name_to_code(std::string(actual_text), &actual_code)) {
// 			// fall back to reading integer directly if keyword unrecognized
// 			actual_code = actual_prop.get_integer();  // documented integer accessor             [1]
// 		}
// 	}

	
//     // --- get expected (current sample from player) ---
//     const char* expected_text = get_value_text().c_str();  // "value" property updated by player
//     if (!expected_text || !*expected_text) {
//         // if empty, you can read a numeric expected from value_code, or soft-skip
//     }
// 	int expected_code = 0;
// 	if (std::isdigit(static_cast<unsigned char>(expected_text[0])) || expected_text[0] == '-') {
// 		expected_code = std::atoi(expected_text);
// 	} else {
// 		if (!map_enum_name_to_code(std::string(expected_text), &expected_code)) {
// 			// optional fallback if your enum files always use keywords
// 			expected_code = get_value_code(); // or soft-skip if you prefer
// 		}
// 	}

	
	
//     if (actual_code != expected_code) {
//         char msg[256];
//         std::snprintf(msg, sizeof(msg),
//                       "Assert failed on %s: %s=%s (%d) did not match %d",
//                       pobj->get_name() ? pobj->get_name() : "(no-name)",
//                       prop_name, actual_text, actual_code, expected_code);
//         // throw msg; // hard fail
//         gl_error("%s", msg);     // soft fail
//         return TS_NEVER;          // keep running
//     }
//     return TS_NEVER;
// }



// TIMESTAMP enum_assert::commit(TIMESTAMP t0, TIMESTAMP t1)
// {
//     gld_object* pobj = get_parent();
//     if (!pobj) throw "enum_assert: parent is null";

//    const TIMESTAMP now = gl_globalclock;
//    char stopbuf[64] = {0};
//    gl_global_getvar("stoptime", stopbuf, sizeof(stopbuf));
//    TIMESTAMP t_stop = gl_parsetime(stopbuf);

//     // --- get actual ---
//     const char* prop_name = target;
//     gld_property actual_prop(pobj->my(), const_cast<char*>(prop_name));
//     if (!actual_prop.is_valid()) { 
// 	    // Fix: Throw an exception or log an error and return.
// 		char msg[256];
// 		snprintf(msg, sizeof(msg), "property '%s' not found in object '%s'", prop_name, pobj->get_name());
// 		gl_error("enum_assert: %s", msg);
// 		return TS_INVALID; // Halt the simulation
			
// 	}
//     char actual_text[64] = {0};
//     actual_prop.to_string(actual_text, sizeof(actual_text));
//     int actual_code = 0;
//     if (std::isdigit(static_cast<unsigned char>(actual_text[0])) || actual_text[0] == '-') {
//         actual_code = std::atoi(actual_text);
//     } else {
//         if (!map_enum_name_to_code(std::string(actual_text), &actual_code)) {
//             actual_code = actual_prop.get_integer(); // integer accessor
//         }
//     }

//     // --- get expected (current sample) ---
//     const char* expected_text = value_text().c_str();
//     int expected_code = 0;
//     if (std::isdigit(static_cast<unsigned char>(expected_text[0])) || expected_text[0] == '-') {
//         expected_code = std::atoi(expected_text);
//     } else {
//         if (!map_enum_name_to_code(std::string(expected_text), &expected_code)) {
//             expected_code = get_value_code();
//         }
//     }

//     if (actual_code != expected_code) {
//         char msg[256];
//         std::snprintf(msg, sizeof(msg),
//             "Assert failed on %s: %s=%s (%d) did not match %d",
//             pobj->get_name() ? pobj->get_name() : "(no-name)",
//             prop_name, actual_text, actual_code, expected_code);
//         gl_error("%s", msg);
//        // Failure semantics: either halt or keep running. Choose one:
//        return TS_INVALID; // <-- halt simulation on failure
//        //return TS_NEVER;     // <-- continue simulation on failure
//     }


//     // --- Adaptive scheduling: snapshot vs continuous ---
//    // When the engine calls commit with advisory/invalid t2, decide what to do.
//    bool continuous = model_is_continuous_like();
//    if (!continuous) {
//        // Snapshot-like: evaluate once and stop
//        return TS_NEVER;
//    } else {
//        // Continuous-like: reschedule at a safe cadence (e.g., 900 s)
//        const TIMESTAMP step = 900; // 15 minutes
//        TIMESTAMP next = now + step;
//        if (t_stop > 0 && next >= t_stop) {
//            return TS_NEVER; // don't overshoot stoptime
//        }
//        // Always return strictly-future timestamps
//        return (next <= now) ? (now + 1) : next;
//    }
// }



TIMESTAMP enum_assert::commit(TIMESTAMP t0, TIMESTAMP t1)
{
    gld_object* pobj = get_parent();
    if (!pobj) throw "enum_assert: parent is null";

    const char* prop_name = target;  // Use raw member
    
    gld_property target_prop(pobj, prop_name);
    if (!target_prop.is_valid() || target_prop.get_type() != PT_enumeration)
    {
        gl_error("enum_assert: target '%s' is not a valid enumeration on '%s'",
                 prop_name, pobj->get_name());
        return TS_INVALID;
    }

    // Get actual value directly as integer (like legacy)
    int32 actual_code;
    target_prop.getp(actual_code);

    // Get expected value
    int expected_code = 0;
    const char* vt = value_text;
    if (std::isdigit(static_cast<unsigned char>(vt[0])) || vt[0] == '-') {
        expected_code = std::atoi(vt);
    } else {
        expected_code = value_code;  // Use pre-parsed code from init()
    }

    if (status == ASSERT_TRUE && actual_code != expected_code)
    {
        gl_error("Assert failed on %s: %s=%d did not match %d",
                 pobj->get_name(), prop_name, actual_code, expected_code);
        return TS_INVALID;
    }

    gl_verbose("Assert passed on %s", pobj->get_name());
    return TS_NEVER;
}

// EXPORT TIMESTAMP commit_enum_assert(OBJECT *obj, TIMESTAMP t1, TIMESTAMP t2)
// {
//     try
//     {
//         return object_data<enum_assert>(obj)->commit(t1, t2);
//     }
//     catch (char *msg)
//     {
//         gl_error("commit_enum_assert(obj=%d;%s): %s", obj->id, obj->name ? obj->name : "unnamed", msg);
//         return TS_INVALID;
//     }
//     catch (const char *msg)
//     {
//         gl_error("commit_enum_assert(obj=%d;%s): %s", obj->id, obj->name ? obj->name : "unnamed", msg);
//         return TS_INVALID;
//     }
//     catch (const std::exception &ex)
//     {
//         gl_error("commit_enum_assert(obj=%d;%s): unhandled exception - %s", obj->id, obj->name ? obj->name : "unnamed", ex.what());
//         return TS_INVALID;
//     }
// }

// Deltamode compatible enumeration assert
EXPORT SIMULATIONMODE update_enum_assert(OBJECT *obj, TIMESTAMP t0, unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{
	char buff[128];
	char dateformat[16] = "";
	char error_output_buff[2028];
	char datebuff[128];
	enum_assert *da = object_data<enum_assert>(obj); /*OBJECTDATA(obj, enum_assert);*/
	DATETIME delta_dt_val;
	double del_clock;
	TIMESTAMP del_clock_int;
	int del_microseconds;
	int32 *x;

	// Iteration checker - assert only valid on the first timestep
	if (iteration_count_val == 0)
	{
		// Skip first timestep of any delta iteration -- nature of delta means it really isn't checking the right one
		if (delta_time >= dt)
		{
			// Get the value
			x = (int32 *)gl_get_enum_by_name(obj->parent, da->get_target().c_str());

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
				if (*x != da->get_value_code())
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
						sprintf(datebuff, "ERROR    %.09f : ", del_clock);

					// Actual error part
					sprintf(error_output_buff, "Assert failed on %s - %s (%d) did not match %d", gl_name(obj->parent, buff, 64), da->get_target().c_str(), *x, da->get_value_code());

					// Send it out
					gl_output("%s%s", datebuff, error_output_buff);

					return SM_ERROR;
				}
				else
				{
					gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
					return SM_EVENT;
				}
			}
			else if (da->get_status() == da->ASSERT_FALSE)
			{
				if (*x == da->get_value_code())
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
						sprintf(datebuff, "ERROR    %.09f : ", del_clock);

					// Actual error part
					sprintf(error_output_buff, "Assert failed on %s - %s (%d) did not match %d", gl_name(obj->parent, buff, 64), da->get_target().c_str(), *x, da->get_value_code());

					// Send it out
					gl_output("%s%s", datebuff, error_output_buff);

					return SM_ERROR;
				}
				else
				{
					gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
					return SM_EVENT;
				}
			}
			else
			{
				gl_verbose("Assert test is not being run on %s", gl_name(obj->parent, buff, 64));
				return SM_EVENT;
			}
		}
		else // first timestep
			return SM_EVENT;
	}
	else // Iteration, so don't care
		return SM_EVENT;
}
