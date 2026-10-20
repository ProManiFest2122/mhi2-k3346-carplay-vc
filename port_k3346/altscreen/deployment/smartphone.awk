# Modify only carplay envs; preserve unrelated service sections.
BEGIN { car=0; found=0; inserted=0; env=0 }
/^[[:space:]]*"carplay"[[:space:]]*:/ {car=1; found++}
car && /^[[:space:]]*"envs"[[:space:]]*:[[:space:]]*\[/ {
  env=1
  print
  print "                \"LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so\","
  inserted++
  next
}
env && /"LD_PRELOAD=/ {next}
env && /^[[:space:]]*\]/ {env=0;car=0}
{print}
END{if(found!=1 || inserted!=1)exit 7}
