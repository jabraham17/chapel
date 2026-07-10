/*
 * Copyright 2020-2026 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*
  Contains a number of helpers to support low-level math routines that the
  runtime might need, for example saturating addition.
*/


/*
  Define a saturating add on signed arithmetic.
  * res, x, and y should be plain identifers of type __stype
  * __utype should be the unsigned version of __stype
  * __smax should be the maximum representable value in __stype
  
  __builtin_elementwise_add_sat is a clang extension, use that if available.
  Its more efficent to use __builtin_add_overflow (gcc/clang) but if not available
  the fall back is OK.
*/
#ifdef __has_builtin
  #if __has_builtin (__builtin_elementwise_add_sat)
    #define CHPL_SAT_SADD(__res, __x, __y, __stype, __utype, __smax) \
      do { \
        __res = __builtin_elementwise_add_sat(__x, __y); \
      } while (0)
  #endif
  #ifndef CHPL_SAT_SADD
      #if __has_builtin (__builtin_add_overflow)
        #define CHPL_SAT_SADD(__res, __x, __y, __stype, __utype, __smax) \
          do { \
            if (__builtin_add_overflow(__x, __y, &__res)) { \
              __res = (__stype)((((__utype)__x) >> (sizeof(__stype)*8-1)) + ((__utype)__smax)); \
            } \
          } while (0)
    #endif
  #endif 
#endif
#ifndef CHPL_SAT_SADD
  #define CHPL_SAT_SADD(__res, __x, __y, __stype, __utype, __smax) \
  do { \
    __utype __ux  = (__utype)__x;
    __utype __uy  = (__utype)__y;
    __utype __ures = __ux + __uy;
    __utype __sat = (__ux >> (sizeof(__stype)*8-1)) + (__utype)__smax;
    __utype __overflow = (__ures ^ __ux) & (__ures ^ __uy);
    __utype __max = (__utype)0 - (__overflow >> (sizeof(__stype)*8-1));
    __res = (__stype)((__ures & ~__max) | (__sat & __max));
  } \
  while (0)
#endif
