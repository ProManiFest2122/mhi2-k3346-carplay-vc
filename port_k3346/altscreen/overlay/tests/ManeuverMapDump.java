/*
 * Exhaustive dump of the Java maneuver mapper - the counterpart of
 * maneuver_map_dump.c.  Same grid, same output format, so the two can be
 * diffed byte for byte.
 *
 * In the same package as ManeuverMapper so it can be compiled against the
 * real class with no shim in between: whatever the car's BAP path does, this
 * dump does.
 */
package com.luka.carplay.routeguidance;

import java.io.PrintStream;
import java.io.BufferedOutputStream;
import java.io.FileOutputStream;

public final class ManeuverMapDump {

    private static final int[] EXTRA_ANGLES = { -1000, 1000, -1 };

    public static void main(String[] args) throws Exception {
        PrintStream out = args.length > 0
            ? new PrintStream(new BufferedOutputStream(new FileOutputStream(args[0]), 1 << 20), false, "US-ASCII")
            : new PrintStream(new BufferedOutputStream(System.out, 1 << 20), false, "US-ASCII");

        for (int type = -2; type <= 55; type++) {
            for (int junction = 0; junction <= 2; junction++) {
                for (int side = 0; side <= 1; side++) {
                    for (int i = 0; i < EXTRA_ANGLES.length; i++) {
                        emit(out, type, EXTRA_ANGLES[i], junction, side);
                    }
                    for (int angle = -180; angle <= 180; angle++) {
                        emit(out, type, angle, junction, side);
                    }
                }
            }
        }
        out.flush();
        if (args.length > 0) out.close();
    }

    private static void emit(PrintStream out, int type, int angle, int junction, int side) {
        int[] r = ManeuverMapper.map(type, angle, junction, side);
        out.print(type);   out.print(' ');
        out.print(angle);  out.print(' ');
        out.print(junction); out.print(' ');
        out.print(side);   out.print(' ');
        out.print(r[0]);   out.print(' ');
        out.print(r[1]);   out.print('\n');
    }
}
