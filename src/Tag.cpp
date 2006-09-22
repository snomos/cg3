/*
 * Copyright (C) 2006, GrammarSoft Aps
 * and the VISL project at the University of Southern Denmark.
 * All Rights Reserved.
 *
 * The contents of this file are subject to the GrammarSoft Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.grammarsoft.com/GSPL or
 * http://visl.sdu.dk/GSPL.txt
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 */
#include "stdafx.h"
#include <unicode/ustring.h>
#include "Strings.h"
#include "Tag.h"
#include "uextras.h"

using namespace CG3;

Tag::Tag() {
	features = 0;
	type = 0;
	comparison_key = 0;
	comparison_op = OP_NOP;
	comparison_val = 0;
	tag = 0;
	regexp = 0;
}

Tag::~Tag() {
	if (tag) {
		delete tag;
	}
	if (comparison_key) {
		delete comparison_key;
	}
}

void Tag::parseTag(const UChar *to) {
	if (to && u_strlen(to)) {
		const UChar *tmp = to;
		while (tmp[0] && (tmp[0] == '!' || tmp[0] == '^')) {
			if (tmp[0] == '!') {
				features |= F_NEGATIVE;
				tmp++;
			}
			if (tmp[0] == '^') {
				features |= F_FAILFAST;
				tmp++;
			}
		}
		// ToDo: Implement these...and numerical tests.
		uint32_t length = u_strlen(tmp);
		if (tmp[0] && (tmp[0] == '"' || tmp[0] == '<')) {
			type |= T_TEXTUAL;
		}
		while (tmp[0] && (tmp[0] == '"' || tmp[0] == '<') && (tmp[length-1] == 'i' || tmp[length-1] == 'r')) {
			if (tmp[length-1] == 'r') {
				features |= F_REGEXP;
				length--;
			}
			if (tmp[length-1] == 'i') {
				features |= F_CASE_INSENSITIVE;
				length--;
			}
		}

		if (tmp[0] == '"' && tmp[length-1] == '"') {
			if (tmp[1] == '<' && tmp[length-2] == '>') {
				type |= T_WORDFORM;
			}
			else {
				type |= T_BASEFORM;
			}
		}

		// ToDo: Implement these...
		if (tmp[0] == 'M' && tmp[1] == 'E' && tmp[2] == 'T' && tmp[3] == 'A' && tmp[4] == ':') {
			type |= T_META;
			tmp += 5;
			length -= 5;
		}
		else if (tmp[0] == 'V' && tmp[1] == 'A' && tmp[2] == 'R' && tmp[3] == ':') {
			type |= T_VARIABLE;
			tmp += 4;
			length -= 4;
		}
		
		tag = new UChar[length+1];
		tag[length] = 0;
		u_strncpy(tag, tmp, length);
		UChar *utag = new UChar[u_strlen(tag)+3];
		ux_unEscape(utag, tag);
		delete tag;
		tag = utag;
		utag = 0;

		if (tag && tag[0] == '<' && tag[length-1] == '>') {
			UChar tkey[256];
			UChar top[256];
			tkey[0] = 0;
			top[0] = 0;
			int tval = 0;
			if (u_sscanf(tag, "<%[^<>=:]%[<>=:]%i>", &tkey, &top, &tval) == 3 && tval != 0 && top[0] && u_strlen(top)) {
				if (top[0] == '<') {
					comparison_op = OP_LESSTHAN;
				}
				else if (top[0] == '>') {
					comparison_op = OP_GREATERTHAN;
				}
				else if (top[0] == '=' || top[0] == ':') {
					comparison_op = OP_EQUALS;
				}
				comparison_val = tval;
				uint32_t length = u_strlen(tkey);
				comparison_key = new UChar[length+1];
				u_strcpy(comparison_key, tkey);
				comparison_hash = hash_sdbm_uchar(comparison_key, 0);
			}
		}

		if (u_strcmp(tag, stringbits[S_ASTERIK]) == 0) {
			type |= T_ANY;
		}
		if (tag[0] == '@') {
			type |= T_MAPPING;
		}

		if (features & F_REGEXP) {
			UParseError *pe = new UParseError;
			UErrorCode status = U_ZERO_ERROR;

			memset(pe, 0, sizeof(UParseError));
			status = U_ZERO_ERROR;
			regexp = uregex_open(tag, u_strlen(tag), 0, pe, &status);
			if (status != U_ZERO_ERROR) {
				u_fprintf(ux_stderr, "Error: uregex_open returned %s trying to parse tag %S!\n", u_errorName(status), tag);
				features &= ~F_REGEXP;
			}
		}
	}
}

void Tag::print(UFILE *to) {
	if (features & F_NEGATIVE) {
		u_fprintf(to, "!");
	}
	if (features & F_FAILFAST) {
		u_fprintf(to, "^");
	}
	if (type & T_META) {
		u_fprintf(to, "META:");
	}
	if (type & T_VARIABLE) {
		u_fprintf(to, "VAR:");
	}

	UChar *tmp = new UChar[u_strlen(tag)*2+3];
	ux_escape(tmp, tag);
	u_fprintf(to, "%S", tmp);
	delete tmp;

	if (features & F_CASE_INSENSITIVE) {
		u_fprintf(to, "i");
	}
	if (features & F_REGEXP) {
		u_fprintf(to, "r");
	}
}

void Tag::printRaw(UFILE *to) {
	if (features & F_NEGATIVE) {
		u_fprintf(to, "!");
	}
	if (features & F_FAILFAST) {
		u_fprintf(to, "^");
	}
	if (type & T_META) {
		u_fprintf(to, "META:");
	}
	if (type & T_VARIABLE) {
		u_fprintf(to, "VAR:");
	}

	u_fprintf(to, "%S", tag);

	if (features & F_CASE_INSENSITIVE) {
		u_fprintf(to, "i");
	}
	if (features & F_REGEXP) {
		u_fprintf(to, "r");
	}
}

uint32_t Tag::rehash() {
	hash = 0;

	if (features & F_NEGATIVE) {
		hash = hash_sdbm_char("!", hash);
	}
	if (features & F_FAILFAST) {
		hash = hash_sdbm_char("^", hash);
	}

	if (type & T_META) {
		hash = hash_sdbm_char("META:", hash);
	}
	if (type & T_VARIABLE) {
		hash = hash_sdbm_char("VAR:", hash);
	}

	UChar *tmp = new UChar[u_strlen(tag)*2+3];
	ux_escape(tmp, tag);
	hash = hash_sdbm_uchar(tmp, hash);
	delete tmp;

	if (features & F_CASE_INSENSITIVE) {
		hash = hash_sdbm_char("i", hash);
	}
	if (features & F_REGEXP) {
		hash = hash_sdbm_char("r", hash);
	}

	return hash;
}

void Tag::duplicateTag(const Tag *from) {
	features = from->features;
	type = from->type;
	hash = from->hash;
	comparison_op = from->comparison_op;
	comparison_val = from->comparison_val;

	if (from->comparison_key) {
		comparison_key = new UChar[u_strlen(from->comparison_key)+1];
		u_strcpy(comparison_key, from->comparison_key);
	}

	if (from->tag) {
		tag = new UChar[u_strlen(from->tag)+1];
		u_strcpy(tag, from->tag);
	}
}