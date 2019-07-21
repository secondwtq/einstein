#ifndef EINSTEIN_MACROS_H
#define EINSTEIN_MACROS_H

#ifdef EINSTEIN_GEN

#define SL_GENERATE [[clang::annotate("SL-generate")]]
#define SL_NON_POD [[clang::annotate("SL-non-pod")]]

#else // EINSTEIN_GEN

#define SL_GENERATE
#define SL_NON_POD

#endif // EINSTEIN_GEN

#endif // EINSTEIN_MACROS_H
