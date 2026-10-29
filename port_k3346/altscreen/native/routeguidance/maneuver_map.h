/*
 * maneuver_map — iAP2 ManeuverType -> BAP (mainElement, direction), in C.
 *
 * WHY A SECOND COPY OF THIS MAPPING
 * The authoritative version is Java: java/build/src/routeguidance/ManeuverMapper.java,
 * which feeds the VC over BAP and has been correct in the car since Etap 4
 * (LSG 50 / FctID 23, "13:64" left / "13:-64" right).  The cluster overlay,
 * though, is fed straight from the hook inside mm-ipod, with no Java in the
 * path - that is the whole point of the Etap 7 design, because the HMI JAR is
 * the one component that can bootloop the car.
 *
 * So the mapping exists twice, and a second copy of a 400-line table is a
 * liability unless something keeps the copies honest.  That something is
 * tests/maneuver_map_diff: it runs this C code and the real Java class over an
 * exhaustive grid of (type, angle, junctionType, drivingSide) and fails on the
 * first disagreement.  Change one side without the other and the build stops.
 *
 * Translation rules followed here:
 *   - same order of tests, same early returns, same constants;
 *   - no floating point, so the hook needs no libm (see dir_from_angle16);
 *   - comments kept where they record a reverse-engineering finding, dropped
 *     where they only restated the code.
 */

#ifndef K3346_MANEUVER_MAP_H
#define K3346_MANEUVER_MAP_H

/* BAP mainElement (CombiBAPConstantsNavi) */
#define BAP_NO_SYMBOL                 0
#define BAP_NO_INFO                   1
#define BAP_ARRIVED                   3
#define BAP_FOLLOW_STREET            11
#define BAP_CHANGE_LANE              12
#define BAP_TURN                     13
#define BAP_EXIT_RIGHT               15
#define BAP_EXIT_LEFT                16
#define BAP_ROUNDABOUT_TRS_RIGHT     21
#define BAP_ROUNDABOUT_TRS_LEFT      22
#define BAP_UTURN                    25
#define BAP_EXIT_ROUNDABOUT_TRS_RIGHT 26
#define BAP_EXIT_ROUNDABOUT_TRS_LEFT  27

/* BAP direction */
#define BAP_DIR_STRAIGHT       0
#define BAP_DIR_SLIGHT_LEFT   32
#define BAP_DIR_LEFT          64
#define BAP_DIR_SHARP_LEFT    96
#define BAP_DIR_UTURN        128
#define BAP_DIR_SHARP_RIGHT  160
#define BAP_DIR_RIGHT        192
#define BAP_DIR_SLIGHT_RIGHT 224

#define JUNCTION_SINGLE_INTERSECTION 0
#define JUNCTION_ROUNDABOUT          1
#define DRIVING_SIDE_RIGHT           0
#define DRIVING_SIDE_LEFT            1

/* iAP2 ManeuverType */
#define MT_NOT_SET                   (-1)
#define MT_NO_TURN                     0
#define MT_LEFT_TURN                   1
#define MT_RIGHT_TURN                  2
#define MT_STRAIGHT_AHEAD              3
#define MT_U_TURN                      4
#define MT_FOLLOW_ROAD                 5
#define MT_ENTER_ROUNDABOUT            6
#define MT_EXIT_ROUNDABOUT             7
#define MT_OFF_RAMP                    8
#define MT_ON_RAMP                     9
#define MT_ARRIVE_END_OF_NAVIGATION   10
#define MT_START_ROUTE                11
#define MT_ARRIVE_AT_DESTINATION      12
#define MT_KEEP_LEFT                  13
#define MT_KEEP_RIGHT                 14
#define MT_ENTER_FERRY                15
#define MT_EXIT_FERRY                 16
#define MT_CHANGE_FERRY               17
#define MT_START_ROUTE_WITH_U_TURN    18
#define MT_U_TURN_AT_ROUNDABOUT       19
#define MT_LEFT_TURN_AT_END           20
#define MT_RIGHT_TURN_AT_END          21
#define MT_HIGHWAY_OFF_RAMP_LEFT      22
#define MT_HIGHWAY_OFF_RAMP_RIGHT     23
#define MT_ARRIVE_DESTINATION_LEFT    24
#define MT_ARRIVE_DESTINATION_RIGHT   25
#define MT_U_TURN_WHEN_POSSIBLE       26
#define MT_ARRIVE_END_OF_DIRECTIONS   27
#define MT_ROUNDABOUT_EXIT_1          28
#define MT_ROUNDABOUT_EXIT_19         46
#define MT_SHARP_LEFT_TURN            47
#define MT_SHARP_RIGHT_TURN           48
#define MT_SLIGHT_LEFT_TURN           49
#define MT_SLIGHT_RIGHT_TURN          50
#define MT_CHANGE_HIGHWAY             51
#define MT_CHANGE_HIGHWAY_LEFT        52
#define MT_CHANGE_HIGHWAY_RIGHT       53

/* Maps one maneuver to the BAP pair.  Writes through both pointers always. */
void k3346_maneuver_map(int maneuver_type, int turn_angle, int junction_type,
                        int driving_side, int *main_element, int *direction);

/* Exposed for the differential test. */
int k3346_direction_from_angle16(int angle);

#endif /* K3346_MANEUVER_MAP_H */
