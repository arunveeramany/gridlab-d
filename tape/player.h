//
// Created by lauraleist on 9/26/18.
//

#ifndef TAPE_PLAYER_H
#define TAPE_PLAYER_H

#include "property.h"
#include "tape.h"

// A new struct to safely link properties with their metadata
struct linked_property {
    PROPERTY *target;       // Pointer to the ORIGINAL property struct
    double scale;           // Scale factor for unit conversion
    int complex_offset;     // -1 for non-complex, otherwise byte offset for Re/Im
    linked_property *next;  // Pointer to the next property in the player's list
};

/** @}
  @addtogroup player
	@{
 **/
class player : public gld_object {
  public:
    char1024 file; //< the name of the player source
    char8 filetype; // the type of the player source
    char256 mode;
    char256 property; //< the target property
    int32 loop; //< the number of time to replay the tape
	bool all_events_delta;	/**< Flag to force any player update to trigger deltamode */

  public: // Should these be private class members?
    union {
        FILE *fp;
        MEMORY *memory;
        void *tsp;
        // add handles for other type of sources as needed
    };
    char lasterr[1024];
    FILETYPE type;
    TIMESTAMP sim_start_time;
    TAPESTATUS status;
    int32 loopnum;
    struct {
        TIMESTAMP ts;
        int64 ns;
        char1024 value;
    } next;
    struct {
        TIMESTAMP ts;
        TIMESTAMP ns;
        char1024 value;
    } delta_track;	// Added for deltamode fixes
    // PROPERTY *target;
    linked_property *target; //< linked list of target properties
    TAPEOPS *ops;

    player(MODULE *module);
    int create(void);
    int init(OBJECT *parent);
    int precommit(TIMESTAMP t0);
    TIMESTAMP commit(TIMESTAMP t0, TIMESTAMP t1);

    static CLASS *oclass;
    static player *defaults;
};

extern TIMESTAMP player_read(OBJECT *obj);

#endif