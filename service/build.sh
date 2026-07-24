#!/bin/sh
set -e

echo "Build Script Version: TROJAN_HORSE_DEDUP_V2"

echo "Installing minimal deps..."
opkg update >/dev/null 2>&1 || true
opkg install gcc make git python3 libstdcpp >/dev/null 2>&1 || true
# Try standard libcurl/sqlite
opkg install libcurl4 libcurl-gnutls libcurl-wolfssl libsqlite3 >/dev/null 2>&1 || true

# 1. Detect System Libs (Musl vs Glibc)
echo "Detecting system linking requirements..."
SYS_LIBS=""

# Check libdl
cat <<EOF > check_dl.cpp
#include <dlfcn.h>
int main() { void *h = dlopen("libc.so", RTLD_LAZY); return 0; }
EOF

if g++ check_dl.cpp -ldl -o check_dl_out >/dev/null 2>&1; then
    echo "  -ldl required."
    SYS_LIBS="$SYS_LIBS -ldl"
elif g++ check_dl.cpp -o check_dl_out >/dev/null 2>&1; then
    echo "  -ldl NOT required (built-in)."
else
    echo "  WARNING: dlopen linking failed. Dynamic loading might be broken."
fi
rm -f check_dl.cpp check_dl_out

# Check libpthread
cat <<EOF > check_pthread.cpp
#include <pthread.h>
int main() { pthread_create(0,0,0,0); return 0; }
EOF

if g++ check_pthread.cpp -lpthread -o check_pthread_out >/dev/null 2>&1; then
    echo "  -lpthread required."
    SYS_LIBS="$SYS_LIBS -lpthread"
elif g++ check_pthread.cpp -o check_pthread_out >/dev/null 2>&1; then
    echo "  -lpthread NOT required (built-in)."
else
    echo "  WARNING: pthread linking failed."
fi
rm -f check_pthread.cpp check_pthread_out

echo "System Libs: $SYS_LIBS"


# 2. Verify Runtime Loading (dlopen)
echo "Verifying runtime loading capability..."
cat <<EOF > test_dlopen.cpp
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    void* handle = dlopen(argv[1], RTLD_LAZY);
    if (!handle) {
        printf("FAIL: %s\n", dlerror());
        return 1;
    }
    printf("SUCCESS: Loaded %s\n", argv[1]);
    dlclose(handle);
    return 0;
}
EOF
g++ test_dlopen.cpp $SYS_LIBS -o test_dlopen || { echo "Failed to build dlopen test"; exit 1; }

# Find real libraries
CURL_LIB=$(find /usr /lib /opt -name "libcurl.so*" 2>/dev/null | grep -v ".py" | head -n 1)
SQL_LIB=$(find /usr /lib /opt -name "libsqlite3.so*" 2>/dev/null | grep -v ".py" | head -n 1)

if [ -z "$CURL_LIB" ]; then echo "No libcurl found."; exit 1; fi
if [ -z "$SQL_LIB" ]; then echo "No libsqlite3 found."; exit 1; fi

echo "Testing $CURL_LIB..."
if ./test_dlopen "$CURL_LIB"; then
    echo "Runtime check passed for libcurl."
else
    echo "Runtime check FAILED for libcurl. The library is truly broken or incompatible."
    exit 1
fi

echo "Testing $SQL_LIB..."
if ./test_dlopen "$SQL_LIB"; then
    echo "Runtime check passed for libsqlite3."
else
    echo "Runtime check FAILED for libsqlite3."
    exit 1
fi

# 3. Create Stubs (Trojan Horse)
echo "Generating stub libraries..."
rm -rf stubs
mkdir -p stubs

# Helper to generate stub source
generate_stub_code() {
    LIB_NAME="$1"
    OUTPUT_SRC="stubs/${LIB_NAME}.c"
    TEMP_SYMS="stubs/${LIB_NAME}_syms.txt"
    
    echo "/* Stub for $LIB_NAME */" > "$OUTPUT_SRC"
    echo "#include <stdlib.h>" >> "$OUTPUT_SRC"
    
    > "$TEMP_SYMS"
    
    # Heuristic + Explicit list
    if [ "$LIB_NAME" = "libcurl" ]; then
        # Collect from grep (might miss macros or overloaded funcs)
        grep -r "curl_" src/ | grep -v ".o" | awk -F'curl_' '{print "curl_" $2}' | awk -F'[^a-zA-Z0-9_]' '{print $1}' >> "$TEMP_SYMS"
        # Add MUST HAVES based on linker error
        echo "curl_global_init" >> "$TEMP_SYMS"
        echo "curl_easy_init" >> "$TEMP_SYMS"
        echo "curl_easy_cleanup" >> "$TEMP_SYMS"
        echo "curl_easy_setopt" >> "$TEMP_SYMS"
        echo "curl_easy_perform" >> "$TEMP_SYMS"
        echo "curl_easy_getinfo" >> "$TEMP_SYMS"
        echo "curl_easy_strerror" >> "$TEMP_SYMS"
        echo "curl_slist_append" >> "$TEMP_SYMS"
        echo "curl_slist_free_all" >> "$TEMP_SYMS"
        echo "curl_version" >> "$TEMP_SYMS"
    else
        grep -r "sqlite3_" src/ | grep -v ".o" | awk -F'sqlite3_' '{print "sqlite3_" $2}' | awk -F'[^a-zA-Z0-9_]' '{print $1}' >> "$TEMP_SYMS"
        # Explicit
        echo "sqlite3_open" >> "$TEMP_SYMS"
        echo "sqlite3_close" >> "$TEMP_SYMS"
        echo "sqlite3_errmsg" >> "$TEMP_SYMS"
        echo "sqlite3_column_text" >> "$TEMP_SYMS"
        echo "sqlite3_column_count" >> "$TEMP_SYMS"
        echo "sqlite3_column_name" >> "$TEMP_SYMS"
        echo "sqlite3_prepare_v2" >> "$TEMP_SYMS"
        echo "sqlite3_step" >> "$TEMP_SYMS"
        echo "sqlite3_finalize" >> "$TEMP_SYMS"
        echo "sqlite3_exec" >> "$TEMP_SYMS"
        echo "sqlite3_busy_timeout" >> "$TEMP_SYMS"
    fi
    
    # Process unique symbols
    cat "$TEMP_SYMS" | sort -u | while read SYM; do
        if [ -z "$SYM" ]; then continue; fi
        # Ensure it's a valid C identifier (simple check)
        if echo "$SYM" | grep -qE "^[a-zA-Z_][a-zA-Z0-9_]*$"; then
            # Special case for varargs like curl_easy_setopt? No, just declare void func.
            # In C linking, function signatures don't matter much unless caller expects return type size match.
            # But void func() {} returns void. Caller expects CURLcode (int).
            # Let's make them return int 0.
            echo "int ${SYM}() { return 0; }" >> "$OUTPUT_SRC"
        fi
    done
    rm -f "$TEMP_SYMS"
}

generate_stub_code "libcurl"
generate_stub_code "libsqlite3"

# Compile Stubs
echo "Compiling stubs..."
# -Wl,-soname is critical.
# We guess libcurl.so.4
gcc -shared -fPIC -Wl,-soname,libcurl.so.4 -o stubs/libcurl.so stubs/libcurl.c
# libsqlite3.so.0 is common
gcc -shared -fPIC -Wl,-soname,libsqlite3.so.0 -o stubs/libsqlite3.so stubs/libsqlite3.c

echo "Stubs created:"
ls -l stubs/*.so

# 4. Link Application
echo "Linking against stubs..."

# Force linking against stubs in ./stubs/
# Add $SYS_LIBS for dl/pthread
STUB_LDFLAGS="-L$(pwd)/stubs -lcurl -lsqlite3 $SYS_LIBS"

# Clean build
rm -rf build src/*.o pc_service

# Compile objects (suppress link for now)
# We use -c to just compile, but make already does that for .o targets.
# Let's just make the .o files explicitly.
log() { printf "\n[%s] %s\n" "$(date '+%F %T')" "$*"; }
log "Compiling source files..."
make src/pc_service.o src/db.o src/utils.o src/sha256.o src/nft_manager.o src/adguard_manager.o src/entertainment_manager.o src/device_manager.o src/license_manager.o src/log_monitor.o src/scheduler.o

# Link manually using the objects
OBJS="src/pc_service.o src/db.o src/utils.o src/sha256.o src/nft_manager.o src/adguard_manager.o src/entertainment_manager.o src/device_manager.o src/license_manager.o src/log_monitor.o src/scheduler.o"
for obj in $OBJS; do
    if [ ! -f "$obj" ]; then
        die "Object file $obj was not created. Compilation failed."
    fi
done

echo "Manual Link Command:"
g++ $OBJS -o pc_service $STUB_LDFLAGS

if [ -f "pc_service" ]; then
    echo "Build SUCCESS (Trojan Horse Method)!"
    # Don't move to subdir, keep in current
    # mkdir -p service
    # mv pc_service service/
    
    echo "---------------------------------------------------"
    echo "To run this binary, ensure LD_LIBRARY_PATH includes real libs."
    echo "For example:"
    echo "export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH"
    echo "./pc_service"
    echo "---------------------------------------------------"
    
    # Optional: verify
    if command -v readelf >/dev/null 2>&1; then
        readelf -d pc_service | grep NEEDED
        echo "Interpreter:"
        readelf -l pc_service | grep interpreter
    fi
else
    echo "Build Failed."
    exit 1
fi
echo "Done."
