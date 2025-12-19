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


// EXPORT_CREATE(enum_assert);
// EXPORT_INIT(enum_assert);
// EXPORT_COMMIT(enum_assert);

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

enum_assert::enum_assert(MODULE *module): gld_object() 
{
		


	if (oclass == nullptr)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		oclass = gl_register_class(module, "enum_assert", sizeof(enum_assert), PC_AUTOLOCK | PC_OBSERVER);
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
								PT_int32, "value", get_value_offset(), PT_DESCRIPTION, "Value to assert",
								PT_char1024, "target", get_target_offset(), PT_DESCRIPTION, "Property to perform the assert upon",
								nullptr) < 1)
		{
			char msg[256];
			sprintf(msg, "unable to publish properties in %s", __FILE__);
			throw msg;
		}

		defaults = this;
		status = ASSERT_TRUE;
		value = 0;
	}
}

/* Object creation is called once for each object that is created by the core */
int enum_assert::create(void)
{
	// memcpy(this, defaults, sizeof(*this));

    if (defaults != nullptr)
    {
        this->status = defaults->status;
        this->value = defaults->value;
        strncpy(this->target, defaults->target, sizeof(this->target) - 1);
    }

	return 1; /* return 1 on success, 0 on failure */
}

int enum_assert::init(OBJECT *parent)
{
	return 1;
}

TIMESTAMP enum_assert::commit(TIMESTAMP t1, TIMESTAMP t2)
{
	// std::cerr << "Commit for enum_assert, parent: " << (void*)get_parent() << ", target: " << get_target().c_str() << std::endl;
    if (!get_parent()) {
        gl_error("Parent object is null for enum_assert");
        return TS_INVALID;
    }
    
	

	// std::cerr << "Commit for enum_assert, parent: " << (void*)get_parent() << ", target: " << get_target().c_str() << std::endl;
	if (!callback) {
        gl_error("FATAL: callback structure is null in enum_assert::commit");
        return TS_INVALID;
    }
	// Check if this is an error test based on parent name
	bool is_error_test = false;
	if (get_parent() && get_parent()->get_name())
	{
		const char *parent_name = get_parent()->get_name();
		is_error_test = strstr(parent_name, "_err") != nullptr;
	}

	std::cerr << "Commit for enum_assert, parent: " << (void*)get_parent() << ", target: " << get_target().c_str() << std::endl;

	gld_property target_prop(get_parent(), get_target().c_str());
	if (!target_prop.is_valid())
    {
        gl_error("Specified target %s for %s is not valid.", get_target().c_str(), get_parent()->get_name());
        return TS_INVALID;
    }
	if (target_prop.get_type() != PT_enumeration)
    {
        gl_error("Specified target %s for %s is not an enumeration type.", get_target().c_str(), get_parent()->get_name());
        return TS_INVALID;
    }
	
	if (!target_prop.is_valid() || target_prop.get_type() != PT_enumeration)
	{
		gl_error("Specified target %s for %s is not valid.", get_target().c_str(), get_parent()->get_name());
		/*  TROUBLESHOOT
		Check to make sure the target you are specifying is a published variable for the object
		that you are pointing to.  Refer to the documentation of the command flag --modhelp, or
		check the wiki page to determine which variables can be published within the object you
		are pointing to with the assert function.
		*/
		return 0;
	}


	int32 x;
	target_prop.getp(x);
	if (status == ASSERT_TRUE)
	{
		if (value != x)
		{
			gl_error("Assert failed on %s: %s=%d did not match %d",
					 get_parent()->get_name(), get_target().c_str(), x, value);
			// Log expected failures for error tests
			if (is_error_test)
			{
				gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
			}

			return TS_INVALID; // Changed from 0 to TS_INVALID
		}
		else
		{
			gl_verbose("Assert passed on %s", get_parent()->get_name());
			return TS_NEVER;
		}
	}
	else if (status == ASSERT_FALSE)
	{
		if (value == x)
		{
			gl_error("Assert failed on %s: %s=%d did match %d",
					 get_parent()->get_name(), get_target().c_str(), x, value);
			// Log expected failures for error tests
			if (is_error_test)
			{
				gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
			}

			return TS_INVALID; // Changed from 0 to TS_INVALID
		}
		else
		{
			gl_verbose("Assert passed on %s", get_parent()->get_name());
			return TS_NEVER;
		}
	}
	else
	{
		gl_verbose("Assert test is not being run on %s", get_parent()->get_name());
		return TS_NEVER;
	}
}

EXPORT int create_enum_assert(OBJECT **obj, OBJECT *parent)
{
    try
    {
        *obj = gl_create_object(enum_assert::oclass);
        if (*obj != NULL)
        {
            enum_assert *my = object_data<enum_assert>(*obj);

			if (!my) {
				gl_error("create_enum_assert: obj->data is null for class 'enum_assert'");
				return 0;
			}


            // gl_set_parent(*obj, parent);
            return my->create();
        }	
        else
            return 0;
    }
    CREATE_CATCHALL(enum_assert);
}

EXPORT TIMESTAMP commit_enum_assert(OBJECT *obj, TIMESTAMP t1, TIMESTAMP t2)
{
    try
    {
        return object_data<enum_assert>(obj)->commit(t1, t2);
    }
    catch (char *msg)
    {
        gl_error("commit_enum_assert(obj=%d;%s): %s", obj->id, obj->name ? obj->name : "unnamed", msg);
        return TS_INVALID;
    }
    catch (const char *msg)
    {
        gl_error("commit_enum_assert(obj=%d;%s): %s", obj->id, obj->name ? obj->name : "unnamed", msg);
        return TS_INVALID;
    }
    catch (const std::exception &ex)
    {
        gl_error("commit_enum_assert(obj=%d;%s): unhandled exception - %s", obj->id, obj->name ? obj->name : "unnamed", ex.what());
        return TS_INVALID;
    }
}

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
				if (*x != da->get_value())
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
					sprintf(error_output_buff, "Assert failed on %s - %s (%d) did not match %d", gl_name(obj->parent, buff, 64), da->get_target().c_str(), *x, da->get_value());

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
				if (*x == da->get_value())
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
					sprintf(error_output_buff, "Assert failed on %s - %s (%d) did not match %d", gl_name(obj->parent, buff, 64), da->get_target().c_str(), *x, da->get_value());

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
