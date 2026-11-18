# Edits only the "envs" list of the "carplay" service in smartphone_integrator.json, so the
# change coexists with other components that edit the same entry (CarPlay VC base, ...).
# Run as: awk -v mode=M -v ours=HOOK_PATH -v codec=CODEC_PATH -f json.awk FILE FILE
#   mode=has    exit 0 both audio entries present, 1 none, 2 only one of them
#   mode=apply  print the file with the entries added (an existing LD_PRELOAD gets ":ours" appended)
#   mode=strip  print the file with exactly those entries removed (inverse of apply)
# Exit 3 apply refused (entries or a foreign codec entry already there), 4 empty envs list,
# 7 unsupported layout (exactly one carplay service with a multi-line envs list is required).
function reset() { car = 0; env = 0; ncar = 0; nenv = 0; items = 0 }
function vend(s, from,   i) { i = index(substr(s, from), "\""); return i ? from + i - 1 : 0 }
BEGIN { pass = 1; rc = 0; ld = 0; hasours = 0; hascodec = 0; othercodec = 0; indent = "                "; reset() }
FNR == 1 && NR != 1 { snc = ncar; sne = nenv; sit = items; pass = 2; reset() }
{
    line = $0
    start = 0
    inenv = 0
    if (line ~ /"carplay"[ \t]*:/) { car = 1; ncar++ }
    else if (car && !env && line ~ /"envs"[ \t]*:[ \t]*\[[ \t\r]*$/) { env = 1; start = 1; nenv++ }
    else if (env && line ~ /^[ \t]*\]/) { env = 0; car = 0 }
    else if (env) { inenv = 1; items++ }
    isld = inenv && index(line, "\"LD_PRELOAD=") > 0
    iscodec = inenv && index(line, "\"AUDIO_SPATIAL_CODEC_LIB=") > 0
    if (pass == 1) {
        if (isld) { ld = 1; if (index(line, ours)) hasours = 1 }
        if (iscodec) { if (index(line, codec)) hascodec = 1; else othercodec = 1 }
        next
    }
    if (mode == "has") next
    if (mode == "apply") {
        if (start) {
            print line
            if (!ld) print indent "\"LD_PRELOAD=" ours "\","
            print indent "\"AUDIO_SPATIAL_CODEC_LIB=" codec "\","
            next
        }
        if (isld) {
            p = index(line, "\"LD_PRELOAD=") + 12
            e = vend(line, p)
            if (!e) { rc = 7; exit }
            print substr(line, 1, e - 1) ":" ours substr(line, e)
            next
        }
        print line
        next
    }
    if (mode == "strip") {
        if (iscodec && index(line, codec)) next
        if (isld && index(line, ours)) {
            p = index(line, "\"LD_PRELOAD=") + 12
            e = vend(line, p)
            if (!e) { rc = 7; exit }
            v = substr(line, p, e - p)
            if (v == ours) next
            n = index(v, ":" ours)
            if (n) w = substr(v, 1, n - 1) substr(v, n + 1 + length(ours))
            else if (index(v, ours ":") == 1) w = substr(v, length(ours) + 2)
            else { rc = 7; exit }
            print substr(line, 1, p - 1) w substr(line, e)
            next
        }
        print line
        next
    }
    rc = 7
    exit
}
END {
    if (rc) exit rc
    if (pass != 2 || snc != 1 || sne != 1 || ncar != 1 || nenv != 1) exit 7
    if (mode == "has") { if (hasours && hascodec) exit 0; if (!hasours && !hascodec) exit 1; exit 2 }
    if (mode == "apply") { if (hasours || hascodec || othercodec) exit 3; if (sit < 1) exit 4 }
    exit 0
}
