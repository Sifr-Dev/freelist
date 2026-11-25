#!/bin/sh
set -eu
stage=${TMPDIR:-/tmp}/freelist-install-check-$$
trap 'rm -rf "$stage"' EXIT HUP INT TERM
make DESTDIR="$stage" PREFIX=/usr install
cat > "$stage/consumer.c" <<'EOF'
#include <freelist.h>
#include <string.h>
int main(void) {
  char *p = my_malloc(8);
  if (!p) return 1;
  memcpy(p, "hello", 6);
  char *q = my_realloc(p, 32);
  if (!q || strcmp(q, "hello") != 0) return 2;
  my_free(q);
  return 0;
}
EOF
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$stage/usr/include" "$stage/consumer.c" "$stage/usr/lib/libfreelist.a" -pthread -o "$stage/consumer"
"$stage/consumer"
make DESTDIR="$stage" PREFIX=/usr uninstall
test ! -e "$stage/usr/lib/libfreelist.a"
test ! -e "$stage/usr/include/freelist.h"
test ! -e "$stage/usr/share/licenses/freelist/LICENSE"
printf '%s\n' 'installed static library and external consumer: PASS'
