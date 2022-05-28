// RUN: %clang -no-canonical-prefixes -target x86_64-unknown-haiku %s -### 2> %t.log
// RUN: FileCheck --check-prefix=CHECK-X86_64 -input-file %t.log %s

// CHECK-X86_64: clang{{.*}}" "-cc1" "-triple" "x86_64-unknown-haiku"
// CHECK-X86_64: gcc{{.*}}" "-o" "a.out" "{{.*}}.o"

// RUN: %clang -no-canonical-prefixes -target i586-pc-haiku %s -### 2> %t.log
// RUN: FileCheck --check-prefix=CHECK-X86 -input-file %t.log %s

// CHECK-X86: clang{{.*}}" "-cc1" "-triple" "i586-pc-haiku"
// CHECK-X86: gcc{{.*}}" "-o" "a.out" "{{.*}}.o"

