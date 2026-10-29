/*
 * maneuver_map — see maneuver_map.h.
 *
 * Line-for-line port of java/build/src/routeguidance/ManeuverMapper.java.
 * Kept in the same order as the Java so the two can be read side by side and
 * so tests/maneuver_map_diff has a fighting chance of staying meaningful.
 */

#include "maneuver_map.h"

/* 0 and +/-1000 are the hook's "no angle" sentinels; -1 is the unset slot default. */
static int has_directional_turn_angle(int a)
{
    return a != 0 && a != 1000 && a != -1000 && a != -1;
}

static int ramp_goes_left(int turn_angle, int driving_side)
{
    if (has_directional_turn_angle(turn_angle)) return turn_angle < 0;
    return driving_side == DRIVING_SIDE_LEFT;
}

/* End-of-road left: default LEFT, shallow angles SLIGHT, beyond -112 SHARP. */
static int dir_end_of_road_left(int exit_angle)
{
    if (exit_angle != 0 && exit_angle < 0 && exit_angle >= -180) {
        if (exit_angle >= -67)  return BAP_DIR_SLIGHT_LEFT;
        if (exit_angle < -112)  return BAP_DIR_SHARP_LEFT;
    }
    return BAP_DIR_LEFT;
}

static int dir_end_of_road_right(int exit_angle)
{
    if (exit_angle != 0 && exit_angle > 0 && exit_angle <= 180) {
        if (exit_angle <= 67)   return BAP_DIR_SLIGHT_RIGHT;
        if (exit_angle > 112)   return BAP_DIR_SHARP_RIGHT;
    }
    return BAP_DIR_RIGHT;
}

static int ramp_direction(int exit_angle, int goes_left)
{
    int absent = (exit_angle == 0 || exit_angle == 1000
                  || exit_angle == -1000 || exit_angle == -1);
    if (goes_left)
        return absent ? BAP_DIR_SLIGHT_LEFT : dir_end_of_road_left(exit_angle);
    return absent ? BAP_DIR_SLIGHT_RIGHT : dir_end_of_road_right(exit_angle);
}

/*
 * Roundabout exits use the reference renderer's 16-step encoding: quantize the
 * exit angle to the nearest 22.5 degree bin in [-180..180], then look the code up.
 *
 * The Java writes floor((a+180)/22.5 + 0.5).  Done here in integers:
 *   floor(x/22.5 + 0.5) == floor((x + 11.25)/22.5) == (4x + 45)/90
 * with x = a + 180 in [0..360], so 4x+45 is always positive and C's truncating
 * division is the floor.  No libm, which keeps the hook's link line unchanged.
 */
int k3346_direction_from_angle16(int angle)
{
    /* rgd_tlv uses +/-1000 when the exit angle is absent.  Clamping that to
     * +/-180 fabricated a U-turn at an otherwise ordinary roundabout exit. */
    static const int DIR16[17] = {
        128, 112, 96, 80, 64, 48, 32, 16, 0, 240, 224, 208, 192, 176, 160, 144, 128
    };
    int a = angle;
    int x, idx;

    if (angle == 1000 || angle == -1000) return BAP_DIR_STRAIGHT;

    if (a >  180) a =  180;
    if (a < -180) a = -180;

    x = a + 180;
    idx = (4 * x + 45) / 90;
    if (idx < 0)  idx = 0;
    if (idx > 16) idx = 16;
    return DIR16[idx];
}

/* START_ROUTE / EXIT_FERRY / CHANGE_HIGHWAY: the override would coarsen the
 * 8-way result anyway, so map straight to the coarse direction. */
static int direction_from_turn_angle(int turn_angle)
{
    if (turn_angle == 1000 || turn_angle == -1000) return BAP_DIR_STRAIGHT;
    if (turn_angle == 0 || turn_angle == -1)       return BAP_DIR_STRAIGHT;
    if (turn_angle < 0)                            return BAP_DIR_LEFT;
    return BAP_DIR_RIGHT;
}

/*
 * The reference BAP implementation coarsens slight/sharp into plain left/right
 * for most actions.  Roundabout and U-turn actions keep their fine direction,
 * and so do ramps/keep/slight/sharp - on the MHI2 VC the distinct icon is the
 * whole point and coarsening SLIGHT to a full 90 degrees defeats it.
 */
static int apply_dsi_nav_bap_direction_override(int mt, int dir)
{
    if (mt == MT_ENTER_ROUNDABOUT
        || mt == MT_EXIT_ROUNDABOUT
        || mt == MT_U_TURN_AT_ROUNDABOUT
        || mt == MT_U_TURN
        || mt == MT_START_ROUTE_WITH_U_TURN
        || mt == MT_U_TURN_WHEN_POSSIBLE
        || (mt >= MT_ROUNDABOUT_EXIT_1 && mt <= MT_ROUNDABOUT_EXIT_19))
        return dir;

    if (mt == MT_OFF_RAMP
        || mt == MT_ON_RAMP
        || mt == MT_HIGHWAY_OFF_RAMP_LEFT
        || mt == MT_HIGHWAY_OFF_RAMP_RIGHT
        || mt == MT_KEEP_LEFT
        || mt == MT_KEEP_RIGHT
        || mt == MT_SLIGHT_LEFT_TURN
        || mt == MT_SLIGHT_RIGHT_TURN
        || mt == MT_SHARP_LEFT_TURN
        || mt == MT_SHARP_RIGHT_TURN)
        return dir;

    if (dir == BAP_DIR_SLIGHT_LEFT || dir == BAP_DIR_LEFT || dir == BAP_DIR_SHARP_LEFT)
        return BAP_DIR_LEFT;
    if (dir == BAP_DIR_SLIGHT_RIGHT || dir == BAP_DIR_RIGHT || dir == BAP_DIR_SHARP_RIGHT)
        return BAP_DIR_RIGHT;
    if (dir == BAP_DIR_UTURN)
        return BAP_DIR_UTURN;
    return BAP_DIR_STRAIGHT;
}

void k3346_maneuver_map(int mt, int turn_angle, int junction_type,
                        int driving_side, int *main_element, int *direction)
{
    int main_el = BAP_NO_INFO;
    int dir = BAP_DIR_STRAIGHT;

    if (mt == MT_NOT_SET) {
        main_el = BAP_NO_SYMBOL;
        dir = BAP_DIR_STRAIGHT;
        goto done;
    }

    /* Global mappings - applied before the junctionType gate. */
    if (mt == MT_NO_TURN || mt == MT_FOLLOW_ROAD) {
        main_el = BAP_FOLLOW_STREET; dir = BAP_DIR_STRAIGHT; goto done;
    }
    if (mt == MT_ARRIVE_END_OF_NAVIGATION || mt == MT_ARRIVE_AT_DESTINATION
        || mt == MT_ARRIVE_END_OF_DIRECTIONS) {
        main_el = BAP_ARRIVED; dir = BAP_DIR_STRAIGHT; goto done;
    }
    if (mt == MT_CHANGE_HIGHWAY_LEFT) {
        main_el = BAP_CHANGE_LANE; dir = BAP_DIR_LEFT; goto done;
    }
    if (mt == MT_CHANGE_HIGHWAY_RIGHT) {
        main_el = BAP_CHANGE_LANE; dir = BAP_DIR_RIGHT; goto done;
    }
    if (mt == MT_ENTER_FERRY || mt == MT_CHANGE_FERRY) {
        /* No BAP mainElement for ferry in this build (constants stop at 38). */
        main_el = BAP_FOLLOW_STREET; dir = BAP_DIR_STRAIGHT; goto done;
    }
    if (mt == MT_START_ROUTE || mt == MT_EXIT_FERRY || mt == MT_CHANGE_HIGHWAY) {
        main_el = BAP_TURN; dir = direction_from_turn_angle(turn_angle); goto done;
    }
    if (mt == MT_U_TURN_AT_ROUNDABOUT) {
        main_el = (driving_side == DRIVING_SIDE_LEFT)
                ? BAP_ROUNDABOUT_TRS_LEFT : BAP_ROUNDABOUT_TRS_RIGHT;
        dir = BAP_DIR_UTURN;
        goto done;
    }
    if (mt == MT_ENTER_ROUNDABOUT) {
        main_el = BAP_TURN;
        dir = (driving_side == DRIVING_SIDE_LEFT) ? BAP_DIR_LEFT : BAP_DIR_RIGHT;
        dir = apply_dsi_nav_bap_direction_override(mt, dir);
        goto done;
    }

    /* junctionType gate: only the roundabout-exit family survives a roundabout
     * junction; anything else there is NO_INFO, as is any other junction type. */
    if (junction_type != JUNCTION_SINGLE_INTERSECTION) {
        if (junction_type != JUNCTION_ROUNDABOUT) {
            main_el = BAP_NO_INFO; dir = BAP_DIR_STRAIGHT; goto done;
        }
        if (mt == MT_EXIT_ROUNDABOUT) {
            main_el = (driving_side == DRIVING_SIDE_LEFT)
                    ? BAP_EXIT_ROUNDABOUT_TRS_LEFT : BAP_EXIT_ROUNDABOUT_TRS_RIGHT;
            dir = (driving_side == DRIVING_SIDE_LEFT) ? BAP_DIR_LEFT : BAP_DIR_RIGHT;
            goto done;   /* override deliberately skipped */
        }
        if (mt >= MT_ROUNDABOUT_EXIT_1 && mt <= MT_ROUNDABOUT_EXIT_19) {
            main_el = (driving_side == DRIVING_SIDE_LEFT)
                    ? BAP_ROUNDABOUT_TRS_LEFT : BAP_ROUNDABOUT_TRS_RIGHT;
            dir = k3346_direction_from_angle16(turn_angle);
            goto done;   /* override deliberately skipped */
        }
        main_el = BAP_NO_INFO; dir = BAP_DIR_STRAIGHT; goto done;
    }

    switch (mt) {
    case MT_STRAIGHT_AHEAD:
        main_el = BAP_TURN; dir = BAP_DIR_STRAIGHT; break;
    case MT_LEFT_TURN:
        main_el = BAP_TURN; dir = BAP_DIR_LEFT; break;
    case MT_RIGHT_TURN:
        main_el = BAP_TURN; dir = BAP_DIR_RIGHT; break;
    case MT_SLIGHT_LEFT_TURN:
    case MT_KEEP_LEFT:
        main_el = BAP_TURN; dir = BAP_DIR_SLIGHT_LEFT; break;
    case MT_SLIGHT_RIGHT_TURN:
    case MT_KEEP_RIGHT:
        main_el = BAP_TURN; dir = BAP_DIR_SLIGHT_RIGHT; break;
    case MT_SHARP_LEFT_TURN:
        main_el = BAP_TURN; dir = BAP_DIR_SHARP_LEFT; break;
    case MT_SHARP_RIGHT_TURN:
        main_el = BAP_TURN; dir = BAP_DIR_SHARP_RIGHT; break;

    case MT_U_TURN:
    case MT_START_ROUTE_WITH_U_TURN:
    case MT_U_TURN_WHEN_POSSIBLE:
        main_el = BAP_UTURN;
        /* The signed exit angle decides the side when present; without it the
         * traffic side keeps both RHT and LHT regions correct. */
        if (has_directional_turn_angle(turn_angle))
            dir = (turn_angle < 0) ? BAP_DIR_LEFT : BAP_DIR_RIGHT;
        else
            dir = (driving_side == DRIVING_SIDE_LEFT) ? BAP_DIR_RIGHT : BAP_DIR_LEFT;
        break;

    case MT_OFF_RAMP:
        /* The signed exit angle drives both side and sharpness; drivingSide is
         * only the fallback for the side. */
        if (ramp_goes_left(turn_angle, driving_side)) {
            main_el = BAP_EXIT_LEFT;  dir = ramp_direction(turn_angle, 1);
        } else {
            main_el = BAP_EXIT_RIGHT; dir = ramp_direction(turn_angle, 0);
        }
        break;

    case MT_ON_RAMP:
        main_el = BAP_TURN;
        dir = ramp_goes_left(turn_angle, driving_side)
            ? BAP_DIR_SLIGHT_LEFT : BAP_DIR_SLIGHT_RIGHT;
        break;

    case MT_HIGHWAY_OFF_RAMP_LEFT:
        main_el = BAP_EXIT_LEFT;  dir = ramp_direction(turn_angle, 1); break;
    case MT_HIGHWAY_OFF_RAMP_RIGHT:
        main_el = BAP_EXIT_RIGHT; dir = ramp_direction(turn_angle, 0); break;

    case MT_ARRIVE_DESTINATION_LEFT:
    case MT_ARRIVE_DESTINATION_RIGHT:
        main_el = BAP_ARRIVED;
        dir = (mt == MT_ARRIVE_DESTINATION_LEFT) ? BAP_DIR_LEFT : BAP_DIR_RIGHT;
        break;

    case MT_LEFT_TURN_AT_END:
        main_el = BAP_TURN; dir = dir_end_of_road_left(turn_angle); break;
    case MT_RIGHT_TURN_AT_END:
        main_el = BAP_TURN; dir = dir_end_of_road_right(turn_angle); break;

    default:
        main_el = BAP_NO_INFO; dir = BAP_DIR_STRAIGHT; break;
    }

    dir = apply_dsi_nav_bap_direction_override(mt, dir);

done:
    if (main_element) *main_element = main_el;
    if (direction)    *direction    = dir;
}
