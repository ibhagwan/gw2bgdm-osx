/*!
 * \brief   The file contains boyer moore search function interface
 * \author  \verbatim
            Created by: Alexander Egorov
            \endverbatim
 * \date    \verbatim
            Creation date: 2010-04-10
            \endverbatim
 * Copyright: (c) Alexander Egorov 2009-2010
*/

#ifndef BMSEARCH_H_
#define BMSEARCH_H_

#ifdef __APPLE__
#include <sys/types.h>
#endif


/*!
 * Searches substring position in a text
 */
long long bm_search(const wchar_t* text, size_t text_length, size_t start_pos, size_t pattern_length,
                 size_t* other_shifts);

/*!
 * Builds substring shift table
 * @param pattern input pattern to build
 * @param pattern_length input pattern length
 * @param other_shifts result shifts
 */
void bm_build(const wchar_t* pattern, size_t pattern_length, size_t* other_shifts);

/*!
 * Does necessary cleanup
 */
void bm_clean();

#endif // BMSEARCH_H_
