/** Assert function
**/

#ifndef _enum_assert_H
#define _enum_assert_H

#include <stdarg.h>
#include <cstddef>

#include "gridlabd.h"

#ifndef _isnan
#define _isnan isnan
#endif

class enum_assert : public gld_object {
public:
	enum {ASSERT_TRUE=1, ASSERT_FALSE, ASSERT_NONE}; 
	
	//GL_ATOMIC(enumeration, status);
	//GL_STRING(char1024,target);
	//GL_ATOMIC(int32,value);

protected:
    enumeration status;  // Member variable of type `enumeration`.
    char1024 target; // Protected member variable
    char1024 value_text;  // expected value as text (e.g., "NONE", "UNDER_VOLTAGE", or "3")
    enumeration value_code;  // parsed numeric code of expected value (internal; also published for numeric feeds)


public:

static inline enum_assert* get_defaults() {
        if (!defaults) {
            defaults = new enum_assert(); // Initialize lazily
        }
        return defaults;
    }

    // Legacy-style offset calculations
    static inline size_t get_status_offset(void) {
        enum_assert* current_defaults = get_defaults();
        return reinterpret_cast<const char*>(&(current_defaults->status)) 
             - reinterpret_cast<const char*>(current_defaults);
    }

    static inline size_t get_target_offset(void) {
        enum_assert* current_defaults = get_defaults();
        return reinterpret_cast<const char*>(&(current_defaults->target)) 
             - reinterpret_cast<const char*>(current_defaults);
    }

    static inline size_t get_value_text_offset(void) {
        enum_assert* current_defaults = get_defaults();
        return reinterpret_cast<const char*>(&(current_defaults->value_text)) 
             - reinterpret_cast<const char*>(current_defaults);
    }

    static inline size_t get_value_code_offset(void) {
        enum_assert* current_defaults = get_defaults();
        return reinterpret_cast<const char*>(&(current_defaults->value_code)) 
             - reinterpret_cast<const char*>(current_defaults);
    }

public:
    // --- Accessors: lock-free & lightweight ---
    // Atomics: relaxed loads/stores (we rely on GridLAB-D phase ordering for happens-before)
    inline enumeration get_status() const noexcept {
        return status;
    }
    inline void set_status(enumeration s) noexcept {
        status = s;
    }

public:
    inline enumeration get_value_code() const noexcept {
        return value_code;
    }
    inline void set_value_code(enumeration v) noexcept {
        value_code = v;
    }

public:
    inline std::string get_target(void) {
        // auto& mtx = SharedMutexManager::get_mutex(my());
        // std::shared_lock<std::shared_mutex> lock(mtx);
        return std::string(target);
    }


    // Write string buffers only during initialization (single-threaded by GLD semantics).
    inline void set_target(const char* str) noexcept {
        if (str) {
            std::strncpy(target, str, sizeof(target) - 1);
            target[sizeof(target) - 1] = '\0';
        } else {
            target[0] = '\0';
        }
    }

public:

    inline std::string get_value_text(void) {
        // auto& mtx = SharedMutexManager::get_mutex(my());
        // std::shared_lock<std::shared_mutex> lock(mtx);
        return std::string(value_text);
    }

    // Write string buffers only during initialization (single-threaded by GLD semantics).
    inline void set_value_text(const char* str) noexcept {
        if (str) {
            std::strncpy(value_text, str, sizeof(value_text) - 1);
            value_text[sizeof(value_text) - 1] = '\0';
        } else {
            value_text[0] = '\0';
        }
    }

    

public:

    
    enum_assert() : status(ASSERT_TRUE), value_code(0) {
        target[0] = '\0';
        value_text[0] = '\0';
    }


    // ~enum_assert() { if (defaults) delete defaults; }

   

public:
	/* required implementations */
	enum_assert(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP commit(TIMESTAMP t1, TIMESTAMP t2);

public:
	static CLASS *oclass;
	static enum_assert *defaults;
};

#endif
