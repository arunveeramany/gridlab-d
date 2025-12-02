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

#include "gld_complex.h"

#include "complex_assert.h"

#include "find.h"
#include <cstring>  

// EXPORT int gld_major = 5;
// EXPORT int gld_minor = 3;


EXPORT_CREATE(complex_assert);
EXPORT_INIT(complex_assert);


// EXPORT CLASS* init(CALLBACKS *fntable, MODULE *mod, int argc, char *argv[])
// {
//     callback = fntable;
// 	std::cerr << "Received callback table in init_enum_assert at address: " << (void*)callback << std::endl;

// 	if (!callback) {
//         std::cerr << "FATAL: complex_enum_assert received null callback table" << std::endl;
//         return 0;
//     }
//     std::cerr << "Callback initialized at address: " << (void*)callback << std::endl;
// 	std::cerr << "properties.get_property callback value: " << (void*)callback->properties.get_property << std::endl;

// 	if (!callback->properties.get_property) {
//         std::cerr << "FATAL: properties.get_property callback is null" << std::endl;
//         return 0;
//     }
// 	std::cerr << "properties.get_property callback initialized at address: " << (void*)callback->properties.get_property << std::endl;

//     new complex_assert(mod); // Instantiate the class to trigger registration
//     // return 1;
// 	return complex_assert::oclass;
// }


EXPORT_COMMIT(complex_assert);
//EXPORT_NOTIFY(complex_assert);

CLASS *complex_assert::oclass = nullptr;
// static complex_assert defaults_storage; // POD storage for defaults
// complex_assert *complex_assert::defaults = &defaults_storage;
extern "C" CALLBACKS *callback;

// EXPORT CLASS* init(CALLBACKS *fntable, MODULE *mod, int argc, char *argv[])
// {
//     callback = fntable;
// 	std::cerr << "Received callback table in complex_enum_assert at address: " << (void*)callback << std::endl;

// 	if (!callback) {
//         std::cerr << "FATAL: init_complex_assert received null callback table" << std::endl;
//         return 0;
//     }
//     std::cerr << "Callback initialized at address: " << (void*)callback << std::endl;
// 	std::cerr << "properties.get_property callback value: " << (void*)callback->properties.get_property << std::endl;

// 	if (!callback->properties.get_property) {
//         std::cerr << "FATAL: properties.get_property callback is null" << std::endl;
//         return 0;
//     }
// 	std::cerr << "properties.get_property callback initialized at address: " << (void*)callback->properties.get_property << std::endl;

//     new complex_assert(mod); // Instantiate the class to trigger registration
//     // return 1;
// 	return complex_assert::oclass;
// }


complex_assert::complex_assert(MODULE *module) : complex_assert() 
{
	if (oclass == nullptr)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		// oclass = gl_register_class(module, "complex_assert", sizeof(complex_assert), PC_AUTOLOCK | PC_OBSERVER);
		oclass = gl_register_class(module, "complex_assert", sizeof(complex_assert), PC_AUTOLOCK );

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
								PT_char1024, "target",  get_target_offset(), PT_DESCRIPTION, "Property to perform the assert upon",
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
		printf("DEBUG: complex_assert constructor - target initialized to: '%s'\n", target);
    	printf("DEBUG: target buffer address: %p\n", &target);

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

	// Use strncpy for safety and ensure null termination.
    strncpy(target, "", sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

	printf("DEBUG: complex_assert::create() - target initialized to: '%s'\n", target);

	return 1; /* return 1 on success, 0 on failure */
}

int complex_assert::init(OBJECT *parent)
{
    printf("*** INIT METHOD CALLED FOR OBJECT %d ***\n", get_id());
    
    // Just store the parent and target name for later resolution
    if (parent == nullptr) {
        printf("ERROR: Parent object is null\n");
        return 0;
    }
    
    // Don't try to resolve properties yet - just validate basic structure
    printf("DEBUG: Deferring property resolution until commit phase\n");
    
    return 1; // Success - actual property resolution happens in commit()
}



// int complex_assert::init(OBJECT *parent)
// {
// 	printf("*** INIT METHOD CALLED FOR OBJECT %d ***\n", get_id());
//     fflush(stdout);

// 	    // Enhanced safety checks for parent object
//     printf("DEBUG: parent=%p\n", parent);
    
//     if (parent == nullptr) {
//         printf("ERROR: Parent object is null\n");
//         return 0;
//     }

// 	// Test 1: Check if parent is valid
//     printf("DEBUG: parent=%p, parent->name=%s\n", parent, parent ? (parent->name ? parent->name : "null") : "null");
    
// 	// Use the callback table to access properties
//     if (callback && callback->properties.get_property) {
//         PROPERTY *test_prop = callback->properties.get_property(parent, "voltage_A", nullptr);
//         printf("DEBUG: callback->properties.get_property returned: %p\n", test_prop);
        
//         if (test_prop) {
//             printf("DEBUG: Property found - type=%d, name=%s\n", test_prop->ptype, test_prop->name);
//         }
//     } else {
//         printf("DEBUG: Callback or get_property is null\n");
//         return 0;
//     }

//     // Test 2: Try gl_get_property with a known property
//     printf("DEBUG: Testing gl_get_property...\n");
//     PROPERTY *test_prop = gl_get_property(parent, "voltage_A");
//     printf("DEBUG: gl_get_property returned: %p\n", test_prop);
    
//     if (test_prop) {
//         printf("DEBUG: Property found - type=%d, name=%s\n", test_prop->ptype, test_prop->name);
//     }
    
//     // Test 3: Try gl_get_addr
//     printf("DEBUG: Testing gl_get_addr...\n");
//     void *test_addr = gl_get_addr(parent, "voltage_A");
//     printf("DEBUG: gl_get_addr returned: %p\n", test_addr);
    
//     fflush(stdout);

// 	gl_verbose("DEBUG: complex_assert::init called for object %d", get_id());
//     gl_verbose("DEBUG: parent=%p, target='%s'", parent, get_target().c_str());

// 	OBJECT *target_obj = nullptr;
//     char obj_name_str[256] = ""; // To hold the parsed object name
//     char prop_name_str[256] = ""; // To hold the parsed property name

//     // Get the full target string from the GLM, e.g., "house_meter.measured_energy"
//     const char* target_str = get_target().c_str();
// 	gl_verbose("DEBUG: target_str='%s'", target_str);

//     // Parse the target string to separate the object name and property name
//     const char *dot = strchr(target_str, '.');
//     if (dot != nullptr)
//     {
//         // Copy the object name part (the part before the dot)
//         size_t obj_name_len = dot - target_str;
//         if (obj_name_len >= sizeof(obj_name_str)) {
//             gl_error("complex_assert: target object name in '%s' is too long", target_str);
//             return 0; // Return 0 for failure
//         }
//         strncpy(obj_name_str, target_str, obj_name_len);
//         obj_name_str[obj_name_len] = '\0'; // Null-terminate the object name

//         // The rest of the string is the property name
//         strcpy(prop_name_str, dot + 1);

//         // Use the correct API to find the object by its name
//         FINDLIST *pFindList = gl_find_objects(FL_NEW, FT_NAME, SAME, obj_name_str, FT_END);

//         // Check if the find operation was successful and if we got exactly one match
//         if (pFindList == nullptr)
//         {
//             gl_error("complex_assert: gl_find_objects failed to run for target '%s'", obj_name_str);
//             return 0;
//         }
//         else if (pFindList->hit_count == 0)
//         {
//             gl_error("complex_assert: target object '%s' not found in the model", obj_name_str);
//             gl_free((void**)&pFindList);
//             return 0;
//         }
//         else if (pFindList->hit_count > 1)
//         {
//             gl_warning("complex_assert: %d objects named '%s' found, using the first one", pFindList->hit_count, obj_name_str);
//         }

//         // Get the first (and only expected) object from the findlist
//         target_obj = gl_find_next(pFindList, nullptr);
// 		gl_free((void**)&pFindList);   
// 	 }
//     else
//     {
//         // Fallback for old syntax: if no dot, the target property is on the immediate parent
//         target_obj = parent;
//         if (target_obj == nullptr) {
//              gl_error("complex_assert: has no parent and target '%s' does not specify an object", target_str);
//              return 0; // Return 0 for failure
//         }
//         strcpy(prop_name_str, target_str);
//     }
	
		
		
//     // 1. Find the PROPERTY structure on the parent
// 	gl_verbose("DEBUG: About to call gl_get_property(target_obj=%p, prop_name='%s')", target_obj, prop_name_str);
//     pTarget = gl_get_property(target_obj, prop_name_str);
//     if (pTarget == nullptr) {
//         gl_error("complex_assert: target property '%s' was not found on parent object '%s'", get_target().c_str(), parent->name);
//         return 0;
//     }
// 	gl_verbose("DEBUG: gl_get_property returned pTarget=%p", pTarget);


//     // 2. Verify the property type
// 	gl_verbose("DEBUG: Property type is %d (PT_complex=%d)", pTarget->ptype, PT_complex);
//     if (pTarget->ptype != PT_complex) {
//         gl_error("complex_assert: target property '%s' is not of type 'complex'", get_target().c_str());
//         return 0;
//     }	

//     // 3. Get the direct memory address of the data and cache it
// 	gl_verbose("DEBUG: About to call gl_get_addr(target_obj=%p, prop_name='%s')", target_obj, prop_name_str);
//     pComplex = (gld::complex*)gl_get_addr(target_obj, prop_name_str);
// 	gl_verbose("DEBUG: gl_get_addr returned pComplex=%p", pComplex);

//     if (pComplex == nullptr) {
//         gl_error("complex_assert: unable to get the address of target property '%s'", get_target().c_str());
//         return 0;
//     }

//     // Check 'within' value
//     if (within <= 0.0)
//     {
//         gl_warning("complex_assert: a non-positive value has been specified for 'within'");
//         // Note: This is a warning, not an error, to allow certain tests.
//     }
	
// 	gl_verbose("DEBUG: complex_assert::init successful, pComplex=%p", pComplex);

// 	return 1; // Success



// }



// TIMESTAMP complex_assert::commit(TIMESTAMP t1, TIMESTAMP t2)
// {

// 	// The target is now resolved in init(). If pComplex is null, init() failed.
//     if (pComplex == nullptr) {
//         return TS_INVALID; // Don't run if initialization failed
//     }

// 	// Get the current value from the cached pointer
//     complex x = *pComplex;


// 	gl_verbose("complex_assert::commit called for %s (operation=%d) on %s",
// 			   get_target().c_str(), operation, get_parent() ? get_parent()->get_name() : "unknown");

// 	// Instead of checking global_modelname, check a local property
// 	bool is_error_test = false;

// 	// Try to determine if this is an error test based on the object or parent name
// 	if (get_parent() && get_parent()->get_name())
// 	{
// 		const char *parent_name = get_parent()->get_name();
// 		is_error_test = strstr(parent_name, "_err") != nullptr;
// 	}

// 	// handle once mode
// 	if (once == ONCE_TRUE)
// 	{
// 		once_value = value;
// 		once = ONCE_DONE;
// 	}
// 	else if (once == ONCE_DONE)
// 	{
// 		if (once_value.Re() == value.Re() && once_value.Im() == value.Im())
// 		{
// 			gl_verbose("Assert skipped with ONCE logic");
// 			return TS_NEVER;
// 		}
// 		else
// 		{
// 			once_value = value;
// 		}
// 	}

// 	// get the target property
// 	// gld_property target_prop(get_parent(),get_target());
// 	gld_property target_prop(get_parent(), get_target().c_str());
// 	if (!target_prop.is_valid() || target_prop.get_type() != PT_complex)
// 	{
// 		gl_error("Specified target %s for %s is not valid.", get_target().c_str(), get_parent()->get_name());
// 		/*  TROUBLESHOOT
// 		Check to make sure the target you are specifying is a published variable for the object
// 		that you are pointing to.  Refer to the documentation of the command flag --modhelp, or
// 		check the wiki page to determine which variables can be published within the object you
// 		are pointing to with the assert function.
// 		*/
// 		return TS_INVALID;
// 	}

// 	// test the target value
// 	//complex x;
// 	//target_prop.getp(x);

// 	if (status == ASSERT_TRUE)
// 	{
// 		// Get the parent object
// 		OBJECT *pParent = get_parent()->my();

// 		// Manually calculate the memory address of the target property's data.
// 		// The property address (pTarget->addr) is a byte offset from the start of the object's data.
// 		void *pPropAddr = (char*)(pParent + 1) + (int64)(pTarget->addr);

// 		// Cast the generic address to a pointer of the correct type (complex*)
// 		complex *y = (complex*)pPropAddr;
// 		complex x = complex(y->Re(), y->Im());
		
// 		if (operation == FULL || operation == REAL || operation == IMAGINARY)
// 		{
// 			complex error = x - value;
// 			double real_error = error.Re();
// 			double imag_error = error.Im();
// 			if ((_isnan(real_error) || fabs(real_error) > within) && (operation == FULL || operation == REAL))
// 			{
// 				gl_error("Assert failed on %s: real part of %s %g not within %f of given value %g",
// 						 get_parent()->get_name(), get_target().c_str(), x.Re(), within, value.Re());
// 				return TS_INVALID;
// 			}
// 			if ((_isnan(imag_error) || fabs(imag_error) > within) && (operation == FULL || operation == IMAGINARY))
// 			{
// 				gl_error("Assert failed on %s: imaginary part of %s %+gi not within %f of given value %+gi",
// 						 get_parent()->get_name(), get_target().c_str(), x.Im(), within, value.Im());
// 				return TS_INVALID;
// 			}
// 		}
// 		else if (operation == MAGNITUDE)
// 		{
// 			double magnitude_error = x.Mag() - value.Mag();
// 			if (_isnan(magnitude_error) || fabs(magnitude_error) > within)
// 			{
// 				gl_error("Assert failed on %s: Magnitude of %s (%g) not within %f of given value %g",
// 						 get_parent()->get_name(), get_target().c_str(), x.Mag(), within, value.Mag());
// 				return TS_INVALID;
// 			}
// 		}
// 		else if (get_operation() == ANGLE)
// 		{
// 			// The expected angle is stored in the real part of the 'value' property.
// 			double expected_angle = value.Re(); 
// 			double actual_angle = x.Arg(); // Angle of the target property
			
// 			// Calculate the absolute difference.
// 			double angle_error = fabs(actual_angle - expected_angle);

// 			// Fail if the error is GREATER THAN the tolerance.
// 			// Also fail if the angle is NaN, which can happen for a 0+0j target.
// 			if (_isnan(actual_angle) || angle_error > within)
// 			{
// 				gl_error("Assert failed on %s: Angle of target %s (%g rad) is not within %f of expected value %g rad",
// 						get_parent()->get_name(), get_target().c_str(), actual_angle, within, expected_angle);
			
// 				// Check if this is an error test
// 				if (is_error_test)
// 				{
// 					gl_verbose("Expected failure in error test file");
// 					// Return TS_INVALID to signal failure without throwing exception
// 					return TS_INVALID;
// 				}
// 				else
// 				{
// 					// For regular tests that fail unexpectedly
// 					gl_error("Unexpected assertion failure in non-error test");
// 					// For non-error tests, still return TS_INVALID
// 					return TS_INVALID;
// 				}
// 			}
// 		}
// 		gl_verbose("Assert passed on %s", get_parent()->get_name());
// 		return TS_NEVER;
// 	}
// 	else if (get_status() == ASSERT_FALSE)
// 	{
// 		if (operation == FULL || operation == REAL || operation == IMAGINARY)
// 		{
// 			complex error = x - value;
// 			double real_error = error.Re();
// 			double imag_error = error.Im();
// 			if ((_isnan(real_error) || fabs(real_error) < within) && (operation == FULL || operation == REAL))
// 			{
// 				gl_error("Assert failed on %s: real part of %s %g is within %f of given value %g",
// 						 get_parent()->get_name(), get_target().c_str(), x.Re(), within, value.Re());
// 				return TS_INVALID;
// 			}
// 			if ((_isnan(imag_error) || fabs(imag_error) < within) && (operation == FULL || operation == IMAGINARY))
// 			{
// 				gl_error("Assert failed on %s: imaginary part of %s %+gi is within %f of given value %+gi",
// 						 get_parent()->get_name(), get_target().c_str(), x.Im(), within, value.Im());
// 				return TS_INVALID;
// 			}
// 		}
// 		else if (operation == MAGNITUDE)
// 		{
// 			double magnitude_error = x.Mag() - value.Mag();
// 			if (_isnan(magnitude_error) || fabs(magnitude_error) < within)
// 			{
// 				gl_error("Assert failed on %s: Magnitude of %s %g is within %f of given value %g",
// 						 get_parent()->get_name(), get_target().c_str(), x.Mag(), within, value.Mag());
// 				return TS_INVALID;
// 			}
// 		}
// 		else if (get_operation() == ANGLE)
// 		{
// 			// The expected angle is stored in the real part of the 'value' property.
// 			double expected_angle = value.Re(); 
// 			double actual_angle = x.Arg(); // Angle of the target property
			
// 			// Calculate the absolute difference.
// 			double angle_error = fabs(actual_angle - expected_angle);
// 			if (_isnan(actual_angle) || angle_error < within)
// 			{
// 				("Assert failed on %s: Angle of target %s (%g rad) IS within %f of expected value %g rad",
//                     get_parent()->get_name(), get_target().c_str(), actual_angle, within, expected_angle);
// 				// return 0;
// 				// Return TS_INVALID instead of 0 to indicate failure properly
// 				return TS_INVALID;
// 			}
// 		}
// 		gl_verbose("Assert passed on %s", get_parent()->get_name());
// 		return TS_NEVER;
// 	}
// 	else
// 	{
// 		gl_verbose("Assert test is not being run on %s", get_parent()->get_name());
// 		return TS_NEVER;
// 	}
// }

// Add a new method for property resolution
int complex_assert::resolve_target_property()
{
    const char* target_str = get_target().c_str();

	printf("DEBUG: target_str raw content: '");
    for(int i = 0; i < 20 && target_str[i] != '\0'; i++) {
        printf("%c", isprint(target_str[i]) ? target_str[i] : '?');
    }
    printf("'\n");
    
    // Add safety check for empty target
    if (strlen(target_str) == 0) {
        gl_error("Target property name is empty");
        return 0;
    }
    
    OBJECT *target_obj = nullptr;
    char obj_name_str[256] = "";
    char prop_name_str[256] = "";
    
    // Parse the target string for dot notation (object.property)
    const char *dot = strchr(target_str, '.');
    if (dot != nullptr) {
        // Extract object name and property name
        size_t obj_name_len = dot - target_str;
        if (obj_name_len >= sizeof(obj_name_str)) {
            gl_error("Target object name in '%s' is too long", target_str);
            return 0;
        }
        strncpy(obj_name_str, target_str, obj_name_len);
        obj_name_str[obj_name_len] = '\0';
        strcpy(prop_name_str, dot + 1);
        
        // Find the object by name
        FINDLIST *pFindList = gl_find_objects(FL_NEW, FT_NAME, SAME, obj_name_str, FT_END);
        if (pFindList == nullptr || pFindList->hit_count == 0) {
            gl_error("Target object '%s' not found", obj_name_str);
            if (pFindList) gl_free((void**)&pFindList);
            return 0;
        }
        target_obj = gl_find_next(pFindList, nullptr);
        gl_free((void**)&pFindList);
    } else {
        // No dot - use parent object with simple property name
        target_obj = get_parent()->my();
        if (target_obj == nullptr) {
            gl_error("complex_assert has no parent and target '%s' doesn't specify an object", target_str);
            return 0;
        }
        strcpy(prop_name_str, target_str);
    }
    
    gl_debug("Resolving target property '%s' on object '%s'", 
             prop_name_str, target_obj->name ? target_obj->name : "unnamed");
             
    // Use target_obj and prop_name_str (not parent and target_str)
    pTarget = gl_get_property(target_obj, prop_name_str);
    if (pTarget == nullptr) {
        gl_error("Property '%s' not found on object '%s'", 
                prop_name_str, target_obj->name ? target_obj->name : "unnamed");
        return 0;
    }
    
    if (pTarget->ptype != PT_complex) {
        gl_error("Property '%s' is not complex type (type=%d)", prop_name_str, pTarget->ptype);
        return 0;
    }
    
    // Use target_obj and prop_name_str (not parent and target_str)
    pComplex = (gld::complex*)gl_get_addr(target_obj, prop_name_str);
    if (pComplex == nullptr) {
        gl_error("Unable to get address of property '%s'", prop_name_str);
        return 0;
    }
    
    gl_debug("Successfully resolved property '%s' at address %p", prop_name_str, pComplex);
    return 1;
}

TIMESTAMP complex_assert::commit(TIMESTAMP t1, TIMESTAMP t2)
{
	    // Resolve properties on first commit (lazy initialization)
    if (pComplex == nullptr) {
        if (resolve_target_property() == 0) {
            return TS_INVALID;
        }
    }

	gl_verbose("DEBUG: Entering complex_assert::commit for object %d", get_id());

	try
	{
		// Add a debug check for the specific problematic object
        if (get_id() == 4) {
            gl_verbose("DEBUG: Processing object 4 (ANGLE operation)");
            gl_verbose("DEBUG: pComplex=%p, operation=%d", pComplex, operation);
        }
		// The target is now resolved in init(). If pComplex is null, init() failed.
		if (pComplex == nullptr)
		{
			return TS_INVALID; // Don't run if initialization failed
		}
		// Add safety check before dereferencing
        gl_verbose("DEBUG: About to dereference pComplex for object %d", get_id());
		// Get the current value from the cached pointer
		complex x = *pComplex;
		gl_verbose("DEBUG: Successfully dereferenced pComplex: %g+%gi", x.Re(), x.Im());

		// Determine if this is an error test based on the parent object's name
		bool is_error_test = false;
		if (get_parent() && get_parent()->get_name())
		{
			const char *parent_name = get_parent()->get_name();
			is_error_test = strstr(parent_name, "_err") != nullptr;
		}

		// Handle 'once' logic
		if (once == ONCE_TRUE)
		{
			once_value = value;
			once = ONCE_DONE;
		}
		else if (once == ONCE_DONE)
		{
			// FIX: Added '==' operators
			if (once_value.Re() == value.Re() && once_value.Im() == value.Im())
			{
				gl_verbose("Assert skipped with ONCE logic for %s", get_parent()->get_name());
				return TS_NEVER;
			}
			else
			{
				once_value = value;
			}
		}

		// --- Main assertion logic ---
		if (status == ASSERT_TRUE)
		{
			// FIX: Added '==' operators
			if (operation == FULL || operation == REAL || operation == IMAGINARY)
			{
				complex error = x - value;
				double real_error = error.Re();
				double imag_error = error.Im();

				// FIX: Added '==' operator
				if ((operation == FULL || operation == REAL) && (_isnan(real_error) || fabs(real_error) > within))
				{
					gl_error("Assert failed on %s: real part of %s (%g) not within %f of given value %g", get_parent()->get_name(), get_target().c_str(), x.Re(), within, value.Re());
					if (is_error_test)
						gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
					return TS_INVALID;
				}
				// FIX: Added '==' operator
				if ((operation == FULL || operation == IMAGINARY) && (_isnan(imag_error) || fabs(imag_error) > within))
				{
					gl_error("Assert failed on %s: imaginary part of %s (%+gi) not within %f of given value %+gi", get_parent()->get_name(), get_target().c_str(), x.Im(), within, value.Im());
					if (is_error_test)
						gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
					return TS_INVALID;
				}
			}
			else if (operation == MAGNITUDE)
			{
				double expected_magnitude = value.Re(); // Correct: Use real part to avoid NaN
				double actual_magnitude = x.Mag();
				double magnitude_error = fabs(actual_magnitude - expected_magnitude);

				if (_isnan(actual_magnitude) || magnitude_error > within)
				{
					gl_error("Assert failed on %s: Magnitude of %s (%g) not within %f of expected value %g", get_parent()->get_name(), get_target().c_str(), actual_magnitude, within, expected_magnitude);
					if (is_error_test)
						gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
					return TS_INVALID;
				}
			}
			else if (operation == ANGLE)
			{
				try
				{
					double expected_angle = value.Re(); // Correct: Use real part to avoid NaN
					double actual_angle = x.Arg();
					double angle_error = fabs(actual_angle - expected_angle);

					if (_isnan(actual_angle) || angle_error > within)
					{
						gl_error("Assert failed on %s: Angle of target %s (%g rad) not within %f of expected value %g rad", get_parent()->get_name(), get_target().c_str(), actual_angle, within, expected_angle);
						if (is_error_test)
							gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
						return TS_INVALID;
					}
				}
				catch (const std::exception &e)
				{
					gl_error("Exception in ANGLE operation for %s: %s", get_parent()->get_name(), e.what());
					return TS_INVALID;
				}
				catch (...)
				{
					gl_error("Unknown exception in ANGLE operation for %s", get_parent()->get_name());
					return TS_INVALID;
				}
			}
		}
		else if (status == ASSERT_FALSE)
		{
			// For ASSERT_FALSE, we fail if the value IS within the tolerance
			// FIX: Added '==' operators
			if (operation == FULL || operation == REAL || operation == IMAGINARY)
			{
				complex error = x - value;
				// FIX: Added '==' operator
				if ((operation == FULL || operation == REAL) && !(_isnan(error.Re()) || fabs(error.Re()) > within))
				{
					gl_error("Assert failed on %s: real part of %s (%g) IS within %f of given value %g", get_parent()->get_name(), get_target().c_str(), x.Re(), within, value.Re());
					if (is_error_test)
						gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
					return TS_INVALID;
				}
				// FIX: Added '==' operator
				if ((operation == FULL || operation == IMAGINARY) && !(_isnan(error.Im()) || fabs(error.Im()) > within))
				{
					gl_error("Assert failed on %s: imaginary part of %s (%+gi) IS within %f of given value %+gi", get_parent()->get_name(), get_target().c_str(), x.Im(), within, value.Im());
					if (is_error_test)
						gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
					return TS_INVALID;
				}
			}
			else if (operation == MAGNITUDE)
			{
				double expected_magnitude = value.Re();
				double actual_magnitude = x.Mag();
				double magnitude_error = fabs(actual_magnitude - expected_magnitude);

				if (!(_isnan(actual_magnitude) || magnitude_error > within))
				{
					gl_error("Assert failed on %s: Magnitude of %s (%g) IS within %f of expected value %g", get_parent()->get_name(), get_target().c_str(), actual_magnitude, within, expected_magnitude);
					if (is_error_test)
						gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
					return TS_INVALID;
				}
			}
			else if (operation == ANGLE)
			{
				try
				{
					double expected_angle = value.Re();
					double actual_angle = x.Arg();
					double angle_error = fabs(actual_angle - expected_angle);

					if (!(_isnan(actual_angle) || angle_error > within))
					{
						gl_error("Assert failed on %s: Angle of target %s (%g rad) IS within %f of expected value %g rad", get_parent()->get_name(), get_target().c_str(), actual_angle, within, expected_angle);
						if (is_error_test)
							gl_verbose("Expected failure in error test object %s", get_parent()->get_name());
						return TS_INVALID;
					}
				}
				catch (const std::exception &e)
				{
					gl_error("Exception in ANGLE operation for %s: %s", get_parent()->get_name(), e.what());
					return TS_INVALID;
				}
				catch (...)
				{
					gl_error("Unknown exception in ANGLE operation for %s", get_parent()->get_name());
					return TS_INVALID;
				}
			}
		}

		gl_verbose("Assert passed on %s", get_parent()->get_name());
		return TS_NEVER; // Return TS_NEVER on success to disable further checks
	}
	catch (const std::exception &e)
	{
		if (get_parent() && get_parent()->get_name())
		{
			gl_error("Exception in complex_assert::commit for %s: %s", get_parent()->get_name(), e.what());
		}
		else
		{
			gl_error("Exception in complex_assert::commit: %s", e.what());
		}
		return TS_INVALID;
	}
	catch (...)
	{
		if (get_parent() && get_parent()->get_name())
		{
			gl_error("Unknown exception in complex_assert::commit for %s", get_parent()->get_name());
		}
		else
		{
			gl_error("Unknown exception in complex_assert::commit");
		}
		return TS_INVALID;
	}
}

// int complex_assert::postnotify(PROPERTY *prop, char *value)
// {
// 	if (once == ONCE_DONE && strcmp(prop->name, "value") == 0)
// 	{
// 		once = ONCE_TRUE;
// 	}
// 	return 1;
// }

EXPORT SIMULATIONMODE update_complex_assert(OBJECT *obj, TIMESTAMP t0, unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{
	char buff[128];
	char dateformat[16] = "";
	char error_output_buff[2048];
	char datebuff[128];
	/*complex_assert *da = OBJECTDATA(obj,complex_assert);*/
	complex_assert *da = object_data<complex_assert>(obj);

	DATETIME delta_dt_val;
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
			else if (da->get_status() == da->ASSERT_TRUE)
			{
				if (da->get_operation() == da->FULL || da->get_operation() == da->REAL || da->get_operation() == da->IMAGINARY)
				{
					complex error = *x - da->get_value();
					double real_error = error.Re();
					double imag_error = error.Im();
					if ((_isnan(real_error) || fabs(real_error) > da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->REAL))
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
						sprintf(error_output_buff, "Assert failed on %s - real part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Re(), da->get_within(), da->get_value().Re());

						// Send it out
						gl_output("%s%s", datebuff, error_output_buff);

						return SM_ERROR;
					}
					if ((_isnan(imag_error) || fabs(imag_error) > da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->IMAGINARY))
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
						sprintf(error_output_buff, "Assert failed on %s - imaginary part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Im(), da->get_within(), da->get_value().Im());

						// Send it out
						gl_output("%s%s", datebuff, error_output_buff);

						return SM_ERROR;
					}
				}
				else if (da->get_operation() == da->MAGNITUDE)
				{
					double magnitude_error = (*x).Mag() - da->get_value().Mag();
					if (_isnan(magnitude_error) || fabs(magnitude_error) > da->get_within())
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
						sprintf(error_output_buff, "Assert failed on %s - Magnitude of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Mag(), da->get_within(), da->get_value().Mag());

						// Send it out
						gl_output("%s%s", datebuff, error_output_buff);

						return SM_ERROR;
					}
				}
				else if (da->get_operation() == da->ANGLE)
				{
					double angle_error = (*x).Arg() - da->get_value().Arg();
					if (_isnan(angle_error) || fabs(angle_error) > da->get_within())
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
					if ((_isnan(real_error) || fabs(real_error) < da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->REAL))
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
						sprintf(error_output_buff, "Assert failed on %s - real part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Re(), da->get_within(), da->get_value().Re());

						// Send it out
						gl_output("%s%s", datebuff, error_output_buff);

						return SM_ERROR;
					}
					if ((_isnan(imag_error) || fabs(imag_error) < da->get_within()) && (da->get_operation() == da->FULL || da->get_operation() == da->IMAGINARY))
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
						sprintf(error_output_buff, "Assert failed on %s - imaginary part of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Im(), da->get_within(), da->get_value().Im());

						// Send it out
						gl_output("%s%s", datebuff, error_output_buff);

						return SM_ERROR;
					}
				}
				else if (da->get_operation() == da->MAGNITUDE)
				{
					double magnitude_error = (*x).Mag() - da->get_value().Mag();
					if (_isnan(magnitude_error) || fabs(magnitude_error) < da->get_within())
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
						sprintf(error_output_buff, "Assert failed on %s - Magnitude of %s (%g) not within %f of given value %g", gl_name(obj->parent, buff, 64), da->get_target().c_str(), (*x).Mag(), da->get_within(), da->get_value().Mag());

						// Send it out
						gl_output("%s%s", datebuff, error_output_buff);

						return SM_ERROR;
					}
				}
				else if (da->get_operation() == da->ANGLE)
				{
					double angle_error = (*x).Arg() - da->get_value().Arg();
					if (_isnan(angle_error) || fabs(angle_error) < da->get_within())
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
