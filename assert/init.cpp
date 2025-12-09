/** $Id: init.cpp 4738 2014-07-03 00:55:39Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
**/

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include<vector>

#include "gridlabd.h"

#include "gld_assert.h"
#include "double_assert.h"
#include "complex_assert.h"
#include "enum_assert.h"
#include "int_assert.h"

// Forward declare the classes your module will register
class complex_assert;
class enum_assert;
class int_assert;
class double_assert;

// 1. DEFINE the global callback pointer here and only here.
CALLBACKS *callback = nullptr;


//std::vector<std::pair<std::unique_ptr<gld_object>, std::string>> allocated_objects;

//template <typename T>
//void register_object(MODULE* module) {
//
//	//std::cout << "Attempting to register type: " << typeid(T).name() << std::endl;
//
//	T* obj = new T(module);
//
//	allocated_objects.emplace_back(std::make_unique<T>(module), typeid(T).name());
//
//
//	//std::cout << "Registered object of type: " << typeid(T).name() << ", at: " << obj << std::endl;
//}


#ifdef _GLD_ASSERT_H
static_assert(true, "gld_assert.h successfully included.");
#endif



EXPORT CLASS* init(CALLBACKS *fntable, MODULE *mod, int argc, char *argv[])
{
    callback = fntable;
	std::cerr << "Received callback table in init_enum_assert at address: " << (void*)callback << std::endl;

	if (!callback) {
        std::cerr << "FATAL: init_enum_assert received null callback table" << std::endl;
        return 0;
    }
    std::cerr << "Callback initialized at address: " << (void*)callback << std::endl;
	std::cerr << "properties.get_property callback value: " << (void*)callback->properties.get_property << std::endl;

	if (!callback->properties.get_property) {
        std::cerr << "FATAL: properties.get_property callback is null" << std::endl;
        return 0;
    }
	std::cerr << "properties.get_property callback initialized at address: " << (void*)callback->properties.get_property << std::endl;

	new g_assert(mod);
    new enum_assert(mod); // Instantiate the class to trigger registration
	new complex_assert(mod);
	new int_assert(mod);
	new double_assert(mod);

    // return 1;
	return double_assert::oclass;
}


// 
// EXPORT CLASS *init(CALLBACKS *fntable, MODULE *module, int argc, char *argv[])
// {
// 	if (set_callback(fntable)==nullptr)
// 	{
// 		errno = EINVAL;
// 		return nullptr;
// 	}

// 	new g_assert(module);
// 	new double_assert(module);
// 	new complex_assert(module);
// 	new enum_assert(module);
//     new int_assert(module);
	

// 	/*register_object <g_assert>(module);
// 	register_object < double_assert>(module);
// 	register_object < complex_assert>(module);
// 	register_object < enum_assert>(module);
// 	register_object < int_assert>(module);*/


// 	/* always return the first class registered */
// 	return g_assert::oclass;
// }


EXPORT int do_kill(void*)
{
	/* if global memory needs to be released, this is a good time to do it */
	return 0;
}

EXPORT int check(){
	/* if any assert objects have bad filenames, they'll fail on init() */
	return 0;
}
