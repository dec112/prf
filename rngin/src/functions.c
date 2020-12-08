/*
 * Copyright (C) 2020  <Wolfgang Kampichler>
 *
 * This file is part of rngin
 *
 * rngin is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rngin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * requires: libyaml-dev, liblog4c-dev, sqlite3
 */

/**
 *  @file    functions.c
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    04-2020
 *  @version 1.0
 *
 *  @brief this file holds policy routing function definitions
 */

/******************************************************************* INCLUDE */

#include "functions.h"

/********************************************************************* CONST */

const char *str_weekday[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const s_attr_t time_attr[] = TIME_ATTRIBUTES;
const s_attr_t queue_attr[] = QUEUE_ATTRIBUTES;

/************************************************* BASE 64 ENCODING/DECODING */

/**
 * base64_decode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */

unsigned char *base64_encode(const unsigned char *src, size_t len,
                             size_t *out_len) {
  unsigned char *out, *pos;
  const unsigned char *end, *in;
  size_t olen;
  int line_len;

  olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
  olen += olen / 72;      /* line feeds */
  olen++;                 /* nul termination */
  if (olen < len)
    return NULL; /* integer overflow */
  out = malloc(olen);
  if (out == NULL)
    return NULL;

  end = src + len;
  in = src;
  pos = out;
  line_len = 0;
  while (end - in >= 3) {
    *pos++ = base64_table[in[0] >> 2];
    *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
    *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
    *pos++ = base64_table[in[2] & 0x3f];
    in += 3;
    line_len += 4;
    if (line_len >= 72) {
      *pos++ = '\n';
      line_len = 0;
    }
  }

  if (end - in) {
    *pos++ = base64_table[in[0] >> 2];
    if (end - in == 1) {
      *pos++ = base64_table[(in[0] & 0x03) << 4];
      *pos++ = '=';
    } else {
      *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
      *pos++ = base64_table[(in[1] & 0x0f) << 2];
    }
    *pos++ = '=';
    line_len += 4;
  }

  if (line_len)
    *pos++ = '\n';

  *pos = '\0';
  if (out_len)
    *out_len = pos - out;
  return out;
}

/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */

unsigned char *base64_decode(const unsigned char *src, size_t len,
                             size_t *out_len) {
  unsigned char dtable[256], *out, *pos, block[4], tmp;
  size_t i, count, olen;
  int pad = 0;

  memset(dtable, 0x80, 256);
  for (i = 0; i < sizeof(base64_table) - 1; i++)
    dtable[base64_table[i]] = (unsigned char)i;
  dtable['='] = 0;

  count = 0;
  for (i = 0; i < len; i++) {
    if (dtable[src[i]] != 0x80)
      count++;
  }

  if (count == 0 || count % 4)
    return NULL;

  olen = count / 4 * 3;
  pos = out = malloc(olen);
  if (out == NULL)
    return NULL;

  count = 0;
  for (i = 0; i < len; i++) {
    tmp = dtable[src[i]];
    if (tmp == 0x80)
      continue;

    if (src[i] == '=')
      pad++;
    block[count] = tmp;
    count++;
    if (count == 4) {
      *pos++ = (block[0] << 2) | (block[1] >> 4);
      *pos++ = (block[1] << 4) | (block[2] >> 2);
      *pos++ = (block[2] << 6) | block[3];
      count = 0;
      if (pad) {
        if (pad == 1)
          pos--;
        else if (pad == 2)
          pos -= 2;
        else {
          /* Invalid padding */
          free(out);
          return NULL;
        }
        break;
      }
    }
  }

  *out_len = pos - out;

  out = realloc(out, *out_len + 2);
  out[*out_len] = '\r';
  out[*out_len + 1] = '\n';

  *out_len += 2;

  return out;
}

/************************************************ STRING AND PARSE FUNCTIONS */

/**
 *  @brief  frees string variable
 *
 *  @arg    char*
 *  @return void
 */

void delete_string(char *ptr) {

  if (ptr != NULL)
    free(ptr);

  ptr = NULL;

  return;
}

/**
 *  @brief  allocate memory and copy string
 *
 *  @arg    char*, size_t
 *  @return char*
 */

char *copy_string(const char *src, size_t len) {

  char *dst = (char *)malloc((len + 1) * sizeof(char));

  if (dst == NULL) {
    LOG4ERROR(pL, "no memory");
    return dst;
  }

  memcpy(dst, src, len);
  dst[len] = '\0';

  return dst;
}

/**
 *  @brief  rallocate memory and replace string
 *
 *  @arg    char*, char*, size_t
 *  @return char*
 */

char *replace_string(char *src, char *new, size_t len) {

  char *dst = (char *)realloc(src, (len + 1) * sizeof(char));

  if (dst == NULL) {
    LOG4ERROR(pL, "no memory");
    return src;
  }

  memcpy(dst, new, len);
  dst[len] = '\0';

  return dst;
}

/**
 *  @brief  parses sip uri
 *
 *  @arg    char*
 *  @return char*
 */

char *extract_sipuri(const char *str) {

  char *ptr = NULL;
  char *ret = NULL;

  size_t len = 0;
  size_t i = 0;

  if (NULL != (ptr = (strstr(str, SIP_URI_SCHEME)))) {
    len = strlen(SIP_URI_SCHEME);
  } else if (NULL != (ptr = (strstr(str, SIPS_URI_SCHEME)))) {
    len = strlen(SIPS_URI_SCHEME);
  } else if (NULL != (ptr = (strstr(str, TEL_URI_SCHEME)))) {
    len = strlen(TEL_URI_SCHEME);
  }

  LOG4DEBUG(pL, "URI  (in): [%s] %ld", str, strlen(str));

  if (len > 0) {
    for (i = len; i < strlen(str); i++) {
      if ((ptr[i] == ':') || (ptr[i] == '>')) {
        break;
      }
    }
    ret = copy_string(ptr, i);
  }

  LOG4DEBUG(pL, "URI (out): [%s] %ld", ret, i);

  return ret;
}

/**
 *  @brief  removes whitespace that my occur in header name / value
 *
 *  @arg    char* size_t*
 *  @return char*
 */

char *trim_string(char *str, size_t *len)
{
	char *end;

	while(isspace(*str))
		str++;

  /* just spaces, return NULL */
	if(*str == 0) {
    *len = 0;
    return NULL;
  }

	end = str + strlen(str) - 1;

	while(end > str && isspace(*end))
		end--;

	*(end + 1) = '\0';

	*len = (end + 1) - str;

	return str;
}


/**
 *  @brief  extract string between p1 and p2
 *
 *  @arg    const char*, const char*, const char*
 *  @return char*
 */

char *extract_string(const char *val, const char *p1, const char *p2) {

  char *ret = NULL;

  if (val == NULL) {
    return ret;
  }

  const char *ptr1 = strstr(val, p1);

  if (ptr1 != NULL) {
    const size_t len1 = strlen(ptr1);
    const char *ptr2 = strstr(ptr1 + len1, p2);

    if (ptr2 != NULL) {
      /* Found both markers, extract text. */
      const size_t len2 = ptr2 - (ptr1 + len1);
      ret = copy_string(ptr1 + len1, len2);
    }
  }

  return ret;
}

/**
 *  @brief  parses a string from yaml line
 *
 *  @arg    char*, size_t
 *  @return char*
 */

char *parse_string(char *val, size_t len, int n) {

  char *ptr = NULL;
  char *str = NULL;

  if (val == NULL) {
    return NULL;
  }

  str = (char *)malloc(len * sizeof(char));
  if (str == NULL) {
    LOG4ERROR(pL, "no memory");
    return NULL;
  }

  if (SCAN_STRING(val, str) != 1) {
    LOG4ERROR(pL, "failed to scan string");
    return NULL;
  }

  ptr = copy_string(str, strlen(str));

  /* cleanup */
  free(str);

  LOG4DEBUG(pL, "[%d]\t[%s]", n, ptr);

  return ptr;
}

/**
 *  @brief  parses an integer from yaml line
 *
 *  @arg    char*
 *  @return int
 */

int parse_integer(char *val, int n) {

  int i = 0;

  if (val == NULL) {
    return 0;
  }

  if (SCAN_INTEGER(val, &i) != 1) {
    LOG4ERROR(pL, "failed to scan integer");
    return 0;
  }

  LOG4DEBUG(pL, "[%d]\t[%d]", n, i);

  return i;
}

/**
 *  @brief  reallocates memory to store next list element
 *
 *  @arg    char*, int
 *  @return s_hdr_t **
 */

s_hdr_t **new_list(s_hdr_t **list, int i) {

  list = (s_hdr_t **)realloc(list, (i + 1) * sizeof(s_hdr_t *));

  if (list == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return list;
}

/**
 *  @brief  initializes list elements
 *
 *  @arg    s_hdr_t*
 *  @return void
 */

void init_list(s_hdr_t *ptr) {

  ptr->name = NULL;
  ptr->value = 0;

  return;
}

/**
 *  @brief  allocates memory for next list item
 *
 *  @arg    void
 *  @return s_hdr_t*
 */

s_hdr_t *new_listitem(void) {

  s_hdr_t *listitem = (s_hdr_t *)malloc(sizeof(s_hdr_t));

  if (listitem == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return listitem;
}

/**
 *  @brief  frees list item memory
 *
 *  @arg    s_hdrlist_t*
 *  @return void
 */

void delete_list(s_hdrlist_t *list) {
  s_hdr_t *ptr = NULL;
  int i;

  if (list != NULL) {
    for (i = 0; i < list->count; i++) {
      ptr = list->header[i];
      LOG4DEBUG(pL, "DELETING [%s] [%s]", ptr->name, ptr->value);
      free(ptr->name);
      free(ptr->value);
      free(ptr);
      ptr = NULL;
    }

    free(list->header);
    free(list);
    list = NULL;
  }

  return;
}

/**
 *  @brief  allocates memory for query attributes
 *
 *  @arg    void
 *  @return s_query_t*
 */

s_query_t *new_query(void) {

  s_query_t *ptr = (s_query_t *)malloc(sizeof(s_query_t));

  if (ptr == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return ptr;
}

/**
 *  @brief  initializes query object
 *
 *  @arg    s_query_t*
 *  @return void
 */

void init_query(s_query_t *ptr) {

  if (ptr != NULL) {
    ptr->max = -1;
    ptr->length = -1;
    ptr->state = NULL;
  }

  return;
}

/**
 *  @brief  deletes query object
 *
 *  @arg    s_query_t*
 *  @return void
 */

void delete_query(s_query_t *ptr) {

  if (ptr != NULL) {
    ptr->max = 0;
    ptr->length = 0;
    if (ptr->state != NULL) {
      free(ptr->state);
      ptr->state = NULL;
    }
    free(ptr);
    ptr = NULL;
  }

  return;
}

/**
 *  @brief  remove all elements from header list
 *
 *  @arg    s_hdrlist_t*
 *  @return int
 */

int remove_list_hdr(s_hdrlist_t *list) {
  int i = 0;
  s_hdr_t *ptr = NULL;

  if (list != NULL) {
    for (i = 0; i < list->count; i++) {
      ptr = list->header[i];
      LOG4DEBUG(pL, "DELETING [%s] [%s]", ptr->name, ptr->value);
      free(ptr->name);
      free(ptr->value);
      free(ptr);
      ptr = NULL;
    }
  }

  list->count = 0;
  free(list->header);
  list->header = NULL;

  return i;
}

/**
 *  @brief  append element to header list
 *
 *  @arg    s_hdrlist_t*, const char*, const char*
 *  @return int
 */

int append_list_hdr(s_hdrlist_t *phdr, const char *name, const char *value) {

  s_hdr_t **plist = NULL;
  int cnt = 0;

  cnt = phdr->count;
  plist = phdr->header;

  plist = new_list(plist, cnt);
  if (plist == NULL) {
    LOG4ERROR(pL, "no memory");
    return -1;
  }
  plist[cnt] = new_listitem();
  if (plist[cnt] == NULL) {
    LOG4ERROR(pL, "no memory");
    phdr->count = cnt;
    phdr->header = plist;
    return -1;
  }

  init_list(plist[cnt]);

  plist[cnt]->name = copy_string(name, strlen(name));
  plist[cnt]->value = copy_string(value, strlen(value));

  phdr->count = cnt + 1;
  phdr->header = plist;

  return 0;
}

/**
 *  @brief  parses CRLF separated header list
 *
 *  @arg    const char*, const char*
 *  @return s_hdrlist_t*
 */

s_hdrlist_t *parse_list_crlf(const char *list, const char *hsep) {

  char *line = NULL;
  char *end = NULL;
  char *tmp = NULL;
  char *buf = NULL;
  char *trim = NULL;
  
  int i = 0;
  int jmp = 0;
  int count = 0;

  s_hdrlist_t *phdr = NULL;
  s_hdr_t **plist = NULL;

  size_t len = 0;

  if (list == NULL) {
    LOG4WARN(pL, "no list content");
    return phdr;
  }

  phdr = (s_hdrlist_t *)malloc(sizeof(s_hdrlist_t));
  if (phdr == NULL) {
    LOG4ERROR(pL, "no memory");
    return phdr;
  }

  while (*list) {
    /* check line length and line end */
    for (i = 0; i < MAX_HDR_LINE; i++) {
      /* Seach for a newline */
      if (list[i]) {
        if ('\r' == list[i]) {
          jmp = 2;
          break;
        }
      }
    }

    if (i == MAX_HDR_LINE) {
      LOG4WARN(pL, "list line exceeds maximum or wrong separator");
      break;
    }
    
    if (NULL == (line = calloc(i + 1, sizeof(char)))) {
      LOG4ERROR(pL, "no memory");
      break;
    }

    memcpy(line, list, i);

    /* check for valid separator */
    if (strstr(line, hsep) == NULL) {
      LOG4WARN(pL, "skipping header line with wrong seperator [%s]", line);
      list = list + (i + jmp);
      free(line);
      continue;
    }

    LOG4DEBUG(pL, "[%d]\t[%s]", count, line);

    tmp = line;
    plist = new_list(plist, count);
    if (plist == NULL) {
      LOG4ERROR(pL, "no memory");
      count = 0;
      plist = NULL;
      break;
    }
    plist[count] = new_listitem();
    while (*line) {
      end = strstr(line, hsep);

      if (end) {
        len = (size_t)(end - line);
        end += strlen(hsep);
        buf = copy_string(line, len);
        trim = trim_string(buf, &len);
        plist[count]->name = copy_string(trim, len);
        free(buf);
        LOG4DEBUG(pL, "-\t[%s]", plist[count]->name);
      } else {
        len = strlen(line);
        end = line + len;
        buf = copy_string(line, len);
        trim = trim_string(buf, &len);
        plist[count]->value = copy_string(trim, len);
        free(buf);
        LOG4DEBUG(pL, "-\t[%s]", plist[count]->value);
      }
      line = end;
    }
    count++;
    list = list + (i + jmp);

    /*cleanup */
    free(tmp);
  }

  phdr->count = count;
  phdr->header = plist;

  return phdr;
}

/**
 *  @brief  parses COMMA separated header list
 *
 *  @arg    const char*, const char*
 *  @return s_hdrlist_t*
 */

s_hdrlist_t *parse_list_comma(const char *value, const char *hsep) {

  char *line = NULL;
  char *tmp = NULL;
  char *end = NULL;
  char *ptr = NULL;
  char *cpy = NULL;

  int count = 0;

  s_hdrlist_t *phdr = NULL;
  s_hdr_t **plist = NULL;

  size_t len = 0;

  if (value == NULL) {
    LOG4WARN(pL, "no list content");
    return phdr;
  }

  phdr = (s_hdrlist_t *)malloc(sizeof(s_hdrlist_t));
  if (phdr == NULL) {
    LOG4ERROR(pL, "no memory");
    return phdr;
  }

  cpy = copy_string(value, strlen(value));
  ptr = strtok(cpy, SEP_COMMA);
  while (ptr != NULL) {
    /* check line length */
    if (strlen(ptr) > MAX_HDR_LINE) {
      LOG4WARN(pL, "list line exceeds maximum");
    }
    /* remove leading whitespace */
    while (*ptr == ' ') {
      ptr++;
    }

    line = copy_string(ptr, strlen(ptr));

    LOG4DEBUG(pL, "[%d]\t[%s]", count, line);

    tmp = line;
    plist = new_list(plist, count);
    if (plist == NULL) {
      LOG4ERROR(pL, "no memory");
      count = 0;
      plist = NULL;
      break;
    }
    plist[count] = new_listitem();
    init_list(plist[count]);
    while (*line) {

      end = strstr(line, hsep);

      if (end) {
        len = (size_t)(end - line);
        end += strlen(hsep);
        plist[count]->name = copy_string(line, len);
        LOG4DEBUG(pL, "-\t[%s]", plist[count]->name);
      } else {
        len = strlen(line);
        end = line + len;
        plist[count]->value = copy_string(line, len);
        LOG4DEBUG(pL, "-\t[%s]", plist[count]->value);
      }
      line = end;
    }
    count++;
    ptr = strtok(NULL, SEP_COMMA);

    /*cleanup */
    free(tmp);
  }

  phdr->count = count;
  phdr->header = plist;

  /* cleanup */
  free(cpy);

  return phdr;
}

/**
 *  @brief  reallocates memory for next rule data
 *
 *  @arg    s_rule_t**, int
 *  @return s_rule_t*
 */

s_rule_t **new_rule(s_rule_t **rule, int i) {

  rule = (s_rule_t **)realloc(rule, (i + 1) * sizeof(s_rule_t *));

  if (rule == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return rule;
}

/**
 *  @brief  reallocates memory for next queue data
 *
 *  @arg    s_queue_t**, int
 *  @return s_queue_t*
 */

s_queue_t **new_queue(s_queue_t **queue, int i) {

  queue = (s_queue_t **)realloc(queue, (i + 1) * sizeof(s_queue_t *));

  if (queue == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return queue;
}

/**
 *  @brief  initializes queue data set
 *
 *  @arg    s_queue_t*
 *  @return void
 */

void init_queue(s_queue_t *ptr) {

  /* attributes */
  ptr->uri = NULL;
  ptr->state = NULL;
  ptr->size = NULL;
  ptr->prio = 0;
}

/**
 *  @brief  frees queue list memory
 *
 *  @arg    s_quelist_t*
 *  @return void
 */

void delete_queue(s_quelist_t *queues) {
  s_queue_t *ptr = NULL;
  int i;

  if (queues != NULL) {
    for (i = 0; i < queues->count; i++) {
      ptr = queues->queue[i];
      LOG4DEBUG(pL, "DELETING QUEUE [%s]", ptr->uri);
      free(ptr->uri);
      free(ptr->state);
      free(ptr->size);
      free(ptr);
      ptr = NULL;
    }

    free(queues->queue);
    free(queues);
    queues = NULL;
  }

  return;
}

/**
 *  @brief  reallocates memory for new rule item
 *
 *  @arg    void
 *  @return s_rule_t*
 */

s_rule_t *new_ruleitem(void) {

  s_rule_t *ruleitem = (s_rule_t *)malloc(sizeof(s_rule_t));

  if (ruleitem == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return ruleitem;
}

/**
 *  @brief  reallocates memory for new queue item
 *
 *  @arg    void
 *  @return s_queu_t*
 */

s_queue_t *new_queueitem(void) {

  s_queue_t *queueitem = (s_queue_t *)malloc(sizeof(s_queue_t));

  if (queueitem == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return queueitem;
}

/**
 *  @brief  initializes rule data set
 *
 *  @arg    s_rule_t*
 *  @return void
 */

void init_rule(s_rule_t *ptr) {

  /* attributes */
  ptr->name = NULL;
  ptr->id = NULL;
  ptr->fallback = NULL;
  ptr->transport = NULL;
  ptr->weekday = NULL;
  ptr->time = NULL;
  ptr->ruri = NULL;
  ptr->header = NULL;
  ptr->next = NULL;
  ptr->add = NULL;
  ptr->route = NULL;
  ptr->valid = TRUE;
  ptr->prio = 0;
  ptr->hits = 0;
  ptr->use = 0;
  /* lists */
  ptr->fblst = NULL;
  ptr->timelst = NULL;
  ptr->hdrlst = NULL;
  ptr->addlst = NULL;
  ptr->quelst = NULL;
}

/**
 *  @brief  deletes rule data set and frees memory
 *
 *  @arg    s_rule_t*
 *  @return void
 */

void delete_rule(s_rulelist_t *rule) {

  s_rule_t *ptr = NULL;
  int i;

  if (rule != NULL) {
    for (i = 0; i < rule->count; i++) {
      ptr = rule->rules[i];
      if (ptr != NULL) {
        LOG4DEBUG(pL, "DELETING === [%s: %s] ===", ptr->id, ptr->name);
        /* attributes */
        delete_string(ptr->name);
        delete_string(ptr->id);
        delete_string(ptr->fallback);
        delete_string(ptr->transport);
        delete_string(ptr->weekday);
        delete_string(ptr->time);
        delete_string(ptr->ruri);
        delete_string(ptr->header);
        delete_string(ptr->next);
        delete_string(ptr->add);
        delete_string(ptr->route);
        /* lists */
        delete_list(ptr->fblst);
        delete_list(ptr->timelst);
        delete_list(ptr->addlst);
        delete_list(ptr->hdrlst);
        /* queues */
        delete_queue(ptr->quelst);
        /* pointer */
        free(ptr);
        ptr = NULL;
      }
    }
    free(rule->rules);
    free(rule);
    rule = NULL;
  }
}

/**
 *  @brief get line scan attributes
 *
 *  @arg    const s_attr_t*, char*
 *  @return const s_attr_t*
 */

const s_attr_t *get_scanner(const s_attr_t *pattr, const char *name) {

  if (name == NULL) {
    return NULL;
  }

  while (pattr != NULL) {
    if (pattr->attr != NULL) {
      if (strcmp(name, pattr->attr) == 0) {
        return pattr;
      }
    }
    pattr++;
  }

  return NULL;
}

/**
 *  @brief get header/list value by name
 *
 *  @arg    s_hdrlist_t*, const char*
 *  @return char*
 */

char *get_listvalbyname(s_hdrlist_t *header, const char *name) {

  int i = 0;
  s_hdr_t **plist;

  if ((header == NULL) || (name == NULL)) {
    return NULL;
  }

  if (header->count == 0) {
    return NULL;
  }

  plist = header->header;

  while (i < header->count) {
    if (plist[i]->name != NULL) {
      if (strcmp(plist[i]->name, name) == 0) {
        return plist[i]->value;
      }
    }
    i++;
  }

  return NULL;
}

/**
 *  @brief get queue index by prio
 *
 *  @arg    s_quelist_t*, const int
 *  @return int
 */

int get_queuebyprio(s_quelist_t *queue, const int prio) {

  int i = 0;
  s_queue_t **pqueue;

  if (queue == NULL) {
    return -1;
  }

  if (queue->count == 0) {
    return -1;
  }

  if (prio > queue->maxprio) {
    return -1;
  }

  pqueue = queue->queue;

  while (i < queue->count) {
    if (prio == pqueue[i]->prio) {
      return i;
    }
    i++;
  }

  return -1;
}

/*********************************************************** CHECK FUNCTIONS */

/**
 *  @brief  check time condition
 *
 *  @arg    char*, char*
 *  @return bool
 */

bool check_time(char *tfrom, char *tto) {

  time_t now;

  double sec_from;
  double sec_to;

  int from_hr;
  int from_min;
  int to_hr;
  int to_min;
  bool ret;

  char delimiter[] = COLON;

  char *ptr = NULL;
  char *str = NULL;

  struct tm beg;

  time(&now);
  beg = *localtime(&now);

  str = tfrom;
  ptr = strtok(str, delimiter);
  from_hr = atoi(ptr);
  ptr = strtok(NULL, delimiter);
  if (ptr != NULL) {
    from_min = atoi(ptr);
  }

  if (tto == NULL) {
    to_hr = from_hr;
    to_min = from_min;
  } else {
    str = tto;
    ptr = strtok(str, delimiter);
    to_hr = atoi(ptr);
    ptr = strtok(NULL, delimiter);
    if (ptr != NULL) {
      to_min = atoi(ptr);
    }
  }

  beg.tm_hour = from_hr;
  beg.tm_min = from_min;
  sec_from = difftime(now, mktime(&beg));

  if (to_hr < from_hr) {
    beg.tm_mday++;
  }

  beg.tm_hour = to_hr;
  beg.tm_min = to_min;
  sec_to = difftime(now, mktime(&beg));

  if ((sec_to <= 0) && (sec_from >= 0)) {
    ret = TRUE;
  } else {
    ret = FALSE;
  }

  return ret;
}

/**
 *  @brief  compare strings or match pattern
 *
 *  @arg    char*, char*
 *  @return bool
 */

bool check_string(const char *name, const char *pattern) {

  if ((name == NULL) || (pattern == NULL)) {
    return TRUE;
  }

  if (*pattern == PREFIX) {
    if (strstr(name, ++pattern) == NULL) {
      return FALSE;
    }
  } else {
    if (strcmp(name, pattern) != 0) {
      return FALSE;
    }
  }
  
  return TRUE;
}

/**
 *  @brief  check queue state condition
 *
 *  @arg    char*, char*
 *  @return bool
 */

bool check_queuestate(const char *state, const char *cur_state) {

  if ((state == NULL) || (cur_state == NULL)) {
    return TRUE;
  }

  if (strcmp(state, cur_state) != 0) {
    return FALSE;
  }

  return TRUE;
}

/**
 *  @brief  check queue size condition
 *
 *  @arg    char*, int, int
 *  @return bool
 */

bool check_queuesize(char *op, int size, int cur_size) {

  if (op == NULL) {
    return TRUE;
  }

  switch (*op) {
  case '=':
    if (cur_size == size) {
      return TRUE;
    } else {
      return FALSE;
    }
    break;
  case '<':
    if (cur_size < size) {
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  case '>':
    if (cur_size > size) {
      return TRUE;
    } else {
      return FALSE;
    }
    break;

  default:
    break;
  }

  return FALSE;
}

/******************************************************* CONDITION FUNCTIONS */

/**
 *  @brief  check weekday condition
 *
 *  @arg    char*
 *  @return bool
 */

bool cond_day(const char *day, s_rule_t *rule) {
  time_t now;
  struct tm beg;

  bool res = TRUE;

  /* do we have something to test */
  if (day == NULL) {
    return res;
  }

  LOG4DEBUG(pL, "--- DAY CHECK...[%s]", rule->id);

  time(&now);
  beg = *localtime(&now);
  if (strstr(day, str_weekday[beg.tm_wday]) != NULL) {
    res = TRUE;
  } else {
    res = FALSE;
  }
  LOG4DEBUG(pL, "%s = %s", day, res ? "TRUE" : "FALSE");

  if (res == TRUE) {
    rule->hits += 1;
  }

  return res;
}

/**
 *  @brief  check next uri condition
 *
 *  @arg    char*, s_rule_t
 *  @return bool
 */

bool cond_nexturi(const char *uri, s_rule_t *rule) {

  bool res = TRUE;
  char *tmp = NULL;

  /* do we have something to test */
  if (uri == NULL) {
    LOG4WARN(pL, "no next uri received");
    return res;
  }

  if (rule->next == NULL) {
    /* no condition: any next uri hits */
    return res;
  }

  LOG4DEBUG(pL, "--- NEXT HOP CHECK...[%s]", rule->id);

  tmp = extract_sipuri(uri);

  if (tmp == NULL) {
    LOG4WARN(pL, "could not extract next uri");
  } else {
    if (check_string(tmp, rule->next)) {
      res = TRUE;
    } else {
      res = FALSE;
    }

    LOG4DEBUG(pL, "%s = %s", rule->next, res ? "TRUE" : "FALSE");

    /* cleanup */
    free(tmp);
  }

  if (res == TRUE) {
    rule->hits += 1;
  }

  return res;
}

/**
 *  @brief  check ruri condition
 *
 *  @arg    char*, s_rule_t
 *  @return bool
 */

bool cond_ruri(const char *ruri, s_rule_t *rule) {

  bool res = TRUE;

  /* do we have something to test */
  if (ruri == NULL) {
    LOG4WARN(pL, "no ruri received");
    return res;
  }

  if (rule->ruri == NULL) {
    /* no condition: any ruri hits */
    return res;
  }

  LOG4DEBUG(pL, "--- RURI CHECK...[%s]", rule->id);

  if (check_string(ruri, rule->ruri)) {
    res = TRUE;
  } else {
    res = FALSE;
  }
  LOG4DEBUG(pL, "%s = %s", rule->ruri, res ? "TRUE" : "FALSE");

  if (res == TRUE) {
    rule->hits += 1;
  }

  return res;
}

/**
 *  @brief  check sip header condition
 *
 *  @arg    s_hdrlist_t*, s_rule_t*, s_hdrlist_t*
 *  @return bool
 */

bool cond_header(s_hdrlist_t *plist, s_rule_t *rule, s_hdrlist_t *shdr) {

  s_hdr_t *hdr = NULL;

  bool res = TRUE;
  bool grp = FALSE;

  char *value = NULL;
  char *name = NULL;

  char empty[] = "empty";

  int i = 0;
  int j = 0;

  /* do we have something to test ?*/
  if (plist == NULL) {
    return res;
  }

  if (plist->count == 0) {
    return res;
  }

  if (shdr == NULL) {
    return res;
  }

  if (shdr->count == 0) {
    return res;
  }

  LOG4DEBUG(pL, "--- HEADER CHECK...[%s]", rule->id);

  name = empty;

  for (i = 0; i < plist->count; i++) {
    hdr = plist->header[i];
    if (hdr) {
      if ((hdr->name != NULL) && (hdr->value != NULL)) {
        value = get_listvalbyname(shdr, hdr->name);
        if (value != NULL) {
          if (check_string(value, hdr->value)) {
            res &= TRUE;
            grp = TRUE;
            j++;
            LOG4DEBUG(pL, "%s: %s = %s", hdr->name, hdr->value, "TRUE");
          } else {
            res &= FALSE;
            LOG4DEBUG(pL, "%s: %s = %s", hdr->name, hdr->value, "FALSE");
          }
          if (strcmp(name, hdr->name) == 0) {
            res |= grp;
          } else if (strcmp(name, empty) != 0) {
            grp = FALSE;
          }
          name = hdr->name;
        }
      }
    }
  }

  if ((res == TRUE) && (i > 0)) {
    rule->hits += j;
  }

  return res;
}

/**
 *  @brief  check queue condition (queries database)
 *
 *  @arg    s_hdrlist_t*, s_rule_t*, s_input_t*, char*
 *  @return bool
 */

bool cond_queue(s_quelist_t *plist, s_rule_t *rule, s_input_t *in, char *uri, const char *dbname) {

  const s_attr_t *scan;

  s_query_t *query = NULL;
  s_hdrlist_t *hlist = NULL;
  s_hdrlist_t *fblist = NULL;

  bool ret = TRUE;
  bool res = TRUE;

  char *suri = NULL;

  int i = 0;
  int rc = 0;
  int idx = -1;
  int prio = 1;

  /* do we have something to test */
  if (plist == NULL) {
    return res;
  }

  LOG4DEBUG(pL, "--- QUEUE CHECK...[%s]", rule->id);

  uri = NULL;
  while (prio <= plist->maxprio) {
    LOG4DEBUG(pL, "\t- using prio: %d", prio);
    idx = get_queuebyprio(plist, prio);

    if (idx == -1) {
      prio++;
      continue;
    }

    if (plist->queue[idx]->uri == NULL) {
      LOG4WARN(pL, "RULE [%s] queue [%d] has no uri", rule->id, idx);
      prio++;
      continue;
    }

    query = new_query();
    if (query == NULL) {
      break;
    }
    init_query(query);

    suri = extract_sipuri(plist->queue[idx]->uri);
    if (suri == NULL) {
      break;
    }
    sqlite_QUERY(query, suri, dbname);
    free(suri);

    if (query->state != NULL) {

      LOG4DEBUG(pL, "\t- size check...[%s]", rule->id);

      if (plist->queue[idx]->size == NULL) {
        res &= TRUE;
      } else {
        scan = get_scanner(queue_attr, "SIZE");
        /* allocate memory */
        char *(val[scan->fields]);
        for (i = 0; i < scan->fields; i++) {
          val[i] = (char *)malloc(strlen(plist->queue[idx]->size) + 1);
          if (val[i] == NULL) {
            LOG4ERROR(pL, "no memory");
            break;
          }
        }

        switch (scan->fields) {
        case 1:
          break;
        case 2:
          /* SIZE '<val' */
          if (sscanf(plist->queue[idx]->size, scan->format, val[0], val[1]) ==
              scan->fields) {
            ret = check_queuesize(val[0], atoi(val[1]), query->length);
            LOG4DEBUG(pL, "%s %s = %s", plist->queue[idx]->uri,
                      plist->queue[idx]->size, ret ? "TRUE" : "FALSE");
            res &= ret;
          }
          break;
        default:;
        }
        /* cleanup */
        for (i = 0; i < scan->fields; i++) {
          if (val[i] != NULL) {
            free(val[i]);
          }
        }
      }

      LOG4DEBUG(pL, "\t- state check...[%s]", rule->id);

      if (plist->queue[idx]->state == NULL) {
        res &= TRUE;
      } else {
        ret = check_queuestate(plist->queue[idx]->state, query->state);
        LOG4DEBUG(pL, "%s %s = %s", plist->queue[idx]->uri,
                  plist->queue[idx]->state, ret ? "TRUE" : "FALSE");
        res &= ret;

        if (res) {
          LOG4DEBUG(pL, "\t- target uri: %s", plist->queue[idx]->uri);
          uri = plist->queue[idx]->uri;
          rule->hits += 1;
          /* cleanup */
          delete_query(query);
          break;
        }
      }
    }

    /* no luck ... have another try with next prio */
    prio++;
    res = TRUE;

    /* cleanup */
    delete_query(query);
  }

  /* nothing found ... try normal next hop if exists */
  if ((uri == NULL) && (in->next != NULL)) {
    LOG4DEBUG(pL, "\t- using normal next hop uri: %s", in->next);
    query = new_query();
    if (query != NULL) {
      init_query(query);
      suri = extract_sipuri(in->next);
      if (suri != NULL) {
        sqlite_QUERY(query, suri, dbname);
        free(suri);
        if (query->state != NULL) {
          ret = check_queuestate("active", query->state);
          LOG4DEBUG(pL, "%s %s = %s", in->next, "active",
                    ret ? "TRUE" : "FALSE");
          if (ret) {
            uri = in->next;
          }
        }
      }

      /* cleanup */
      delete_query(query);
    }
  }

  /* still nothing ... return fallback uri */
  if (uri == NULL) {
    uri = get_listvalbyname(rule->fblst, ROUTE);
    if (uri == NULL) {
      LOG4ERROR(pL, "no fallback route defined");
    } else {
      LOG4DEBUG(pL, "\t- using fallback uri: %s", uri);
      LOG4WARN(pL, "no active queue found, using fallback: %s", uri);
      if (rule->fblst != NULL) {
        fblist = rule->fblst;
        if (fblist->count > 1) {
          LOG4WARN(pL, "replacing 'add action' header list with default");
          if (rule->addlst == NULL) {
            rule->addlst = (s_hdrlist_t *)malloc(sizeof(s_hdrlist_t));
            if (rule->addlst == NULL) {
              LOG4ERROR(pL, "no memory");
              LOG4WARN(pL, "can not replace 'add action' header");
            } else {
              hlist = rule->addlst;
              hlist->header = NULL;
              hlist->count = 0;
            }
          } else {
            hlist = rule->addlst;
          }
          if (hlist != NULL) {
            rc = remove_list_hdr(hlist);
            LOG4DEBUG(pL, "\t- %d removed", rc);
            for (i = 1; i < fblist->count; i++) {
              append_list_hdr(hlist, fblist->header[i]->name,
                              fblist->header[i]->value);
              LOG4DEBUG(pL, "\t- adding %s: %s", fblist->header[i]->name,
                        fblist->header[i]->value);
            }
          }
        }
      }
    }
  }

  return res;
}

/**
 *  @brief  check time condition
 *
 *  @arg    s_hdrlist_t*, s_rule_t*
 *  @return bool
 */

bool cond_time(s_hdrlist_t *plist, s_rule_t *rule) {

  const s_attr_t *scan;

  s_hdr_t *hdr = NULL;

  bool ret = FALSE;
  bool res = FALSE;

  int i = 0;
  int j = 0;

  /* time has up to two params */
  char *(val[2]);

  /* do we have something to test */
  if (plist == NULL) {
    /* no condition: any time hits */
    res = TRUE;
    return res;
  }

  if (plist->count == 0) {
    /* no condition list: any time hits */
    res = TRUE;
    return res;
  }

  LOG4DEBUG(pL, "--- TIME CHECK...[%s]", rule->id);

  for (j = 0; j < plist->count; j++) {
    hdr = plist->header[j];
    if (hdr) {
      scan = get_scanner(time_attr, hdr->name);
      /* allocate memory */
      for (i = 0; i < scan->fields; i++) {
        val[i] = (char *)malloc(strlen(hdr->value) + 1);
        if (val[i] == NULL) {
          LOG4ERROR(pL, "no memory");
          res = TRUE;
        }
      }

      switch (scan->fields) {
      case 1:
        /* TIME hh:mm */
        if (sscanf(hdr->value, scan->format, val[0]) == scan->fields) {
          if (strlen(hdr->value) != strlen(scan->str)) {
            LOG4WARN(pL,
                     "warning: rule %s has wrong attribute [%s] change to "
                     "[%s]\n",
                     rule->id, hdr->value, scan->str);
          }
          ret = check_time(val[0], NULL);
          LOG4DEBUG(pL, "%s = %s", hdr->value, ret ? "TRUE" : "FALSE");
          res |= ret;
        }
        break;
      case 2:
        /* RANGE hh:mm-hh:mm */
        if (sscanf(hdr->value, scan->format, val[0], val[1]) == scan->fields) {
          if (strlen(hdr->value) != strlen(scan->str)) {
            LOG4WARN(pL,
                     "warning: rule %s has wrong attribute [%s] change to "
                     "[%s]\n",
                     rule->id, hdr->value, scan->str);
          }
          ret = check_time(val[0], val[1]);
          LOG4DEBUG(pL, "%s = %s", hdr->value, ret ? "TRUE" : "FALSE");
          res |= ret;
        }
        break;
      default:;
      }

      /* cleanup */
      for (i = 0; i < scan->fields; i++) {
        if (val[i] != NULL) {
          free(val[i]);
        }
      }
    }
  }

  if (res == TRUE) {
    rule->hits += 1;
  }

  return res;
}

/**
 *  @brief  check if route can be set
 *
 *  @arg    s_hdrlist_t*, s_rule_t*, s_input_t*, char*
 *  @return bool
 */

bool cond_setroute(s_quelist_t *plist, s_rule_t *rule, s_input_t *in,
                   char *uri) {

  s_hdrlist_t *hlist = NULL;
  char *suri = NULL;
  char *tmp = NULL;

  size_t len;

  bool res = TRUE;

  LOG4DEBUG(pL, "--- SET ROUTE...[%s]", rule->id);

  /* didn't get uri ... use route action as next hop */
  if (uri == NULL) {
    uri = rule->route;
  }

  /* didn't get uri from route action ... use normal next hop */
  if (uri == NULL) {
    uri = in->next;
    rule->route = replace_string(rule->route, uri, strlen(uri));
  }

  /* otherwise mark rule as invalid */
  if (uri == NULL) {
    LOG4WARN(pL, "no route target defined in [%s] -> invalid", rule->id);
    res = FALSE;
    return res;
  }

  LOG4DEBUG(pL, "routing to next hop: '%s'", uri);

  if (!check_string(uri, in->next)) {
    suri = extract_sipuri(in->next);
    if (suri == NULL) {
      LOG4WARN(pL, "could not get normal next hop uri: %s", in->next);
    } else {
      len = strlen(suri) + strlen(HIDX0);
      tmp = (char *)malloc(len + 1);
      if (tmp == NULL) {
        LOG4ERROR(pL, "no memory");
        LOG4WARN(pL, "could not add history info uri: %s", in->next);
      } else {
        snprintf(tmp, len, HIDX0, suri);
        if (rule->addlst != NULL) {
          hlist = rule->addlst;
          append_list_hdr(hlist, HINFO, tmp);
          LOG4DEBUG(pL, "\t- adding H-I header: %s", tmp);
        }
        /* cleanup */
        free(tmp);
      }
      /* cleanup */
      free(suri);
    }
  }

  if ((strstr(rule->route, ";transport") == NULL) &&
      (rule->transport != NULL)) {
    len = strlen(rule->route) + strlen(rule->transport) + strlen(TPSTR);
    tmp = (char *)malloc(len + 1);
    if (tmp == NULL) {
      LOG4ERROR(pL, "no memory");
    } else {
      snprintf(tmp, len, TPSTR, rule->route, rule->transport);
      rule->route = replace_string(rule->route, tmp, strlen(tmp));
      /* cleanup */
      free(tmp);
    }
  }

  return res;
}

/***************************************************** YAML PARSER FUNCTIONS */

/**
 *  @brief  set yaml parser state
 *
 *  @arg    const char*, *int*
 *  @return void
 */

void set_state(const char *key, int *state) {

  if NODE_RULE (key) {
    *state = S_RULE;
  } else if NODE_RUID (key) {
    *state = S_RUID;
  } else if NODE_PRIO (key) {
    *state = S_PRIO;
  } else if NODE_DFLT (key) {
    *state = S_DFLT;
  } else if NODE_TRPT (key) {
    *state = S_TRPT;
  } else if NODE_COND (key) {
    *state = S_COND;
  } else if NODE_WEEK (key) {
    *state = S_WEEK;
  } else if NODE_TIME (key) {
    *state = S_TIME;
  } else if NODE_RURI (key) {
    *state = S_RURI;
  } else if NODE_SHDR (key) {
    *state = S_SHDR;
  } else if NODE_NEXT (key) {
    *state = S_NEXT;
  } else if NODE_QUEUE (key) {
    *state = S_QUEUE;
  } else if NODE_QSURI (key) {
    *state = S_QSURI;
  } else if NODE_QSTAT (key) {
    *state = S_QSTAT;
  } else if NODE_QSIZE (key) {
    *state = S_QSIZE;
  } else if NODE_QPRIO (key) {
    *state = S_QPRIO;
  } else if NODE_ADD (key) {
    *state = S_ADD;
  } else if NODE_RTE (key) {
    *state = S_RTE;
  } else
    *state = S_NONE;
}

/**
 *  @brief  reads and parses yaml file (rules ..)
 *
 *  @arg    char*
 *  @return s_rulelist_t*
 */

s_rulelist_t *parse_rule(const char *file) {
  s_rulelist_t *rlist = NULL;
  s_rule_t **prule = NULL;
  s_queue_t **pqueue = NULL;

  FILE *fh = fopen(file, "r");

  char *key = NULL;
  char *val = NULL;

  int state = 0;
  int qstate = 0;
  int n = -1;
  int q = -1;

  bool iskey = FALSE;

  yaml_parser_t parser;
  yaml_token_t token;
  size_t len = 0;

  if (fh == NULL) {
    LOG4ERROR(pL, "can not open rule file [%s]", file);
    return NULL;
  }

  rlist = (s_rulelist_t *)malloc(sizeof(s_rulelist_t));

  if (rlist == NULL) {
    LOG4ERROR(pL, "no memory");
    return rlist;
  }

  rlist->maxhits = 0;
  rlist->maxprio = 0;

  /* Initialize parser */
  if (!yaml_parser_initialize(&parser))
    LOG4ERROR(pL, "failed to initialize parser [%d]", errno);
  if (fh == NULL)
    LOG4ERROR(pL, "failed to open file %s [%d]", file, errno);

  /* Set input file */
  yaml_parser_set_input_file(&parser, fh);

  do {
    if (!yaml_parser_scan(&parser, &token)) {
      LOG4WARN(pL, "yml scan failed");
    }
    switch (token.type) {
    /* Stream start/end */
    case YAML_STREAM_START_TOKEN:
      break;
    case YAML_STREAM_END_TOKEN:
      break;
    /* Token types (read before actual token) */
    case YAML_KEY_TOKEN:
      iskey = TRUE;
      break;
    case YAML_VALUE_TOKEN:
      iskey = FALSE;
      break;
    /* Block delimiters */
    case YAML_BLOCK_SEQUENCE_START_TOKEN:
      break;
    case YAML_BLOCK_ENTRY_TOKEN:
      /* INIT QUEUE LIST*/
      if (state == S_QUEUE) {
        LOG4DEBUG(pL, ">> QUEUES");
        q = -1;
        pqueue = NULL;
        if (prule[n]) {
          prule[n]->quelst = (s_quelist_t *)malloc(sizeof(s_quelist_t));
          if (prule[n]->quelst == NULL) {
            LOG4ERROR(pL, "no memory");
            break;
          }
        }
        prule[n]->quelst->maxprio = 0;
      }
      break;
    case YAML_BLOCK_END_TOKEN:
      break;
    /* Data */
    case YAML_BLOCK_MAPPING_START_TOKEN:
      break;
    case YAML_SCALAR_TOKEN:
      /* FIND KEYS */
      if (iskey) {
        /* GET KEYS */
        key = (char *)token.data.scalar.value;
        set_state(key, &state);
      } else {
        /* PARSE VALUES */
        if (state > S_NONE) {
          val = (char *)token.data.scalar.value;
          len = strlen(val) + 1;
        }
        /* RULE NAME */
        if (len > 1) {
          if (state == S_RULE) {
            LOG4DEBUG(pL, ">>> RULE <<<");
            prule = new_rule(prule, ++n);
            if (prule == NULL) {
              rlist->count = 0;
              rlist->rules = NULL;
              break;
            }
            prule[n] = new_ruleitem();
            if (prule[n] == NULL) {
              rlist->count = n;
              rlist->rules = prule;
              break;
            }
            init_rule(prule[n]);
            prule[n]->name = parse_string(val, len, n);
            state = S_NONE;
          }
          /* RULE ID */
          if (state == S_RUID) {
            LOG4DEBUG(pL, ">> ID");
            prule[n]->id = parse_string(val, len, n);
            state = S_NONE;
          }
          /* RULE PRIO */
          if (state == S_PRIO) {
            LOG4DEBUG(pL, ">> PRIO");
            prule[n]->prio = parse_integer(val, n);
            state = S_NONE;
          }
          /* RULE FALLBACK */
          if (state == S_DFLT) {
            LOG4DEBUG(pL, ">> DEFAULT");
            prule[n]->fallback = parse_string(val, len, n);
            prule[n]->fblst = parse_list_comma(prule[n]->fallback, SEP_HDR);
            state = S_NONE;
          }
          /* RULE TRANSPORT */
          if (state == S_TRPT) {
            LOG4DEBUG(pL, ">> TRANSPORT");
            prule[n]->transport = parse_string(val, len, n);
            state = S_NONE;
          }
          /* CONDITION WEEKDAY */
          if (state == S_WEEK) {
            LOG4DEBUG(pL, ">> DAY");
            prule[n]->weekday = parse_string(val, len, n);
            state = S_NONE;
          }
          /* CONDITION RURI */
          if (state == S_RURI) {
            LOG4DEBUG(pL, ">> RURI");
            prule[n]->ruri = parse_string(val, len, n);
            state = S_NONE;
          }
          /* CONDITION TIME */
          if (state == S_TIME) {
            LOG4DEBUG(pL, ">> TIME");
            prule[n]->time = parse_string(val, len, n);
            prule[n]->timelst = parse_list_comma(prule[n]->time, SEP_SPACE);
            state = S_NONE;
          }
          /* CONDITION SIP HEADER */
          if (state == S_SHDR) {
            LOG4DEBUG(pL, ">> HDR");
            prule[n]->header = parse_string(val, len, n);
            prule[n]->hdrlst = parse_list_comma(prule[n]->header, SEP_HDR);
            state = S_NONE;
          }
          /* CONDITION NEXT HOP */
          if (state == S_NEXT) {
            LOG4DEBUG(pL, ">> NEXT");
            prule[n]->next = parse_string(val, len, n);
            state = S_NONE;
          }
          /* CONDITION QUEUE URI */
          if (state == S_QSURI) {
            pqueue = new_queue(pqueue, ++q);
            if (pqueue == NULL) {
              LOG4ERROR(pL, "no memory");
              prule[n]->quelst->count = 0;
              prule[n]->quelst->queue = NULL;
              break;
            }
            pqueue[q] = new_queueitem();
            if (pqueue[q] == NULL) {
              prule[n]->quelst->count = q;
              prule[n]->quelst->queue = pqueue;
              break;
            }
            init_queue(pqueue[q]);
            pqueue[q]->uri = parse_string(val, len, q);

            prule[n]->quelst->count = q + 1;
            prule[n]->quelst->queue = pqueue;
            qstate = S_QUEUE;
            state = S_NONE;
          }
          /* CONDITION QUEUE STATE */
          if ((state == S_QSTAT) && (qstate == S_QUEUE)) {
            pqueue[q]->state = parse_string(val, len, q);
            state = S_NONE;
          }
          /* CONDITION QUEUE SIZE */
          if ((state == S_QSIZE) && (qstate == S_QUEUE)) {
            pqueue[q]->size = parse_string(val, len, q);
            state = S_NONE;
          }
          /* CONDITION QUEUE PRIO */
          if ((state == S_QPRIO) && (qstate == S_QUEUE)) {
            pqueue[q]->prio = parse_integer(val, q);
            state = S_NONE;
            qstate = S_NONE;
            if (pqueue[q]->prio > prule[n]->quelst->maxprio) {
              prule[n]->quelst->maxprio = pqueue[q]->prio;
            }
          }
          /* ACTION ADD */
          if (state == S_ADD) {
            LOG4DEBUG(pL, ">> ADD");
            prule[n]->add = parse_string(val, len, n);
            prule[n]->addlst = parse_list_comma(prule[n]->add, SEP_HDR);
          }
          /* ACTION ROUTE */
          if (state == S_RTE) {
            LOG4DEBUG(pL, ">> ROUTE");
            prule[n]->route = parse_string(val, len, n);
          }
        }
      }
    /* Others */
    default:;
    }
    if (token.type != YAML_STREAM_END_TOKEN)
      yaml_token_delete(&token);

  } while (token.type != YAML_STREAM_END_TOKEN);

  rlist->count = n + 1;
  rlist->rules = prule;

  /* free token and cleanup */
  yaml_token_delete(&token);
  yaml_parser_delete(&parser);

  fclose(fh);

  if (qstate != S_NONE) {
    LOG4ERROR(pL, "wrong configuration file [%s]", file);
    return NULL;
  }

  return rlist;
}

/**
 *  @brief  print all yaml rules
 *
 *  @arg    s_rulelist_t*, bool
 *  @return void
 */

void print_rule(s_rulelist_t *rulelist, bool printall) {

  s_hdrlist_t *plist = NULL;
  s_quelist_t *pqueue = NULL;
  s_hdr_t **phdr = NULL;
  s_queue_t **pque = NULL;
  s_rule_t **rules = NULL;

  int i;
  int j;

  if (rulelist != NULL) {
    if (rulelist->rules != NULL) {
      if (rulelist->count > 0) {
        printf("########## max. prio: %d / max. hits: %d ###\n",
               rulelist->maxprio, rulelist->maxhits);
        rules = rulelist->rules;
        for (i = 0; i < rulelist->count; i++) {
          if (rules[i] != NULL) {
            if (((rules[i]->valid) && (rules[i]->use == 1)) || printall) {
              printf("    RULE: %s [%s] [%d]\n"
                     "      ID: [%s]\n"
                     "    PRIO: [%d]\n"
                     "   DEFLT: [%s]\n"
                     "  TRANSP: [%s]\n"
                     "    WEEK: [%s]\n"
                     "    TIME: [%s]\n"
                     "    RURI: [%s]\n"
                     "  SIPHDR: [%s]\n"
                     "    NEXT: [%s]\n"
                     ">>   ADD: [%s]\n"
                     ">> ROUTE: [%s]\n",
                     rules[i]->name, rules[i]->valid ? "valid" : "invalid",
                     rules[i]->hits, rules[i]->id, rules[i]->prio,
                     rules[i]->fallback, rules[i]->transport, rules[i]->weekday,
                     rules[i]->time, rules[i]->ruri, rules[i]->header,
                     rules[i]->next, rules[i]->add, rules[i]->route);
              if (rules[i]->addlst != NULL) {
                plist = rules[i]->addlst;
                if (plist->header != NULL) {
                  printf("------------\nADD cnt: %d\n", plist->count);
                  phdr = plist->header;
                  for (j = 0; j < plist->count; j++) {
                    printf("name: %s\n", phdr[j]->name);
                    printf("value: %s\n", phdr[j]->value);
                  }
                }
              }

              if (rules[i]->hdrlst != NULL) {
                plist = rules[i]->hdrlst;
                if (plist->header != NULL) {
                  printf("------------\nHDR cnt: %d\n", plist->count);
                  phdr = plist->header;
                  for (j = 0; j < plist->count; j++) {
                    printf("name: %s\n", phdr[j]->name);
                    printf("value: %s\n", phdr[j]->value);
                  }
                }
              }

              if (rules[i]->quelst != NULL) {
                pqueue = rules[i]->quelst;
                if (pqueue->queue != NULL) {
                  printf("------------\nQUEUE cnt: %d / max. prio: %d\n",
                         pqueue->count, pqueue->maxprio);
                  pque = pqueue->queue;
                  for (j = 0; j < pqueue->count; j++) {
                    printf("uri: %s\n", pque[j]->uri);
                    printf("state: %s\n", pque[j]->state);
                    printf("size: %s\n", pque[j]->size);
                    printf("prio: %d\n", pque[j]->prio);
                  }
                }
              }

              if (rules[i]->timelst != NULL) {
                plist = rules[i]->timelst;
                if (plist->header != NULL) {
                  printf("------------\nTIME cnt: %d\n", plist->count);
                  phdr = plist->header;
                  for (j = 0; j < plist->count; j++) {
                    printf("name: %s\n", phdr[j]->name);
                    printf("value: %s\n", phdr[j]->value);
                  }
                }
              }
              printf("############\n");
            }
          }
        }
      }
    }
  }
}

/**
 *  @brief  execute condition validation on each rule
 *
 *  @arg    s_input_t*, s_rulelist_t*, s_hdrlist_t*
 *  @return void
 */

void validate_rule(s_input_t *cond, s_rulelist_t *rule, s_hdrlist_t *shdr, const char* dbname) {

  s_rule_t **rules = NULL;
  char *uri = NULL;
  int i;

  if (rule != NULL) {
    if (rule->rules != NULL) {
      rules = rule->rules;
      for (i = 0; i < rule->count; i++) {
        if (rules[i] != NULL) {
          rules[i]->valid &= cond_ruri(cond->ruri, rules[i]);
          rules[i]->valid &= cond_nexturi(cond->next, rules[i]);
          rules[i]->valid &= cond_day(rules[i]->weekday, rules[i]);
          rules[i]->valid &= cond_time(rules[i]->timelst, rules[i]);
          rules[i]->valid &= cond_header(rules[i]->hdrlst, rules[i], shdr);
          /* execute condition validation only for valid rules */
          if (rules[i]->valid) {
            rules[i]->valid |=
                cond_queue(rules[i]->quelst, rules[i], cond, uri, dbname);
          }
          if (rules[i]->valid) {
            /* if we can't set a route, rule gets invalid */
            rules[i]->valid &=
                cond_setroute(rules[i]->quelst, rules[i], cond, uri);
          }

          if (rules[i]->valid) {
            if (rules[i]->prio > rule->maxprio) {
              rule->maxprio = rules[i]->prio;
            }
            if (rules[i]->hits > rule->maxhits) {
              rule->maxhits = rules[i]->hits;
            }
          }
        }
      }
    }
  }
}

/**
 *  @brief  selects a valid rule based on prio and condition hits
 *
 *  @arg    s_input_t*, s_rulelist_t*, s_hdrlist_t*
 *  @return void
 */

void select_rule(s_input_t *cond, s_rulelist_t *rule, s_hdrlist_t *shdr) {

  s_rule_t **rules = NULL;
  int i = 0;
  int count = 0;
  int lastindex = 0;

  (void)cond;
  (void)shdr;

  LOG4DEBUG(pL, "=== RULE SELECTION ===");

  if (rule != NULL) {
    if (rule->rules != NULL) {
      rules = rule->rules;
      for (i = 0; i < rule->count; i++) {
        if (rules[i] != NULL) {
          if (rules[i]->valid) {
            LOG4DEBUG(pL, "[prio:%d hit:%d] => [%s]", rules[i]->prio,
                      rules[i]->hits, rules[i]->id);
            /* preselect rules: most specific */
            if (rules[i]->hits == rule->maxhits) {
              rules[i]->use = 1;
              count++;
              LOG4DEBUG(pL, "SELECTED RULE [%s] => hit count", rules[i]->id);
            }
          }
        }
      }

      if (count > 1) {
        i = 0;
        while (i < rule->count) {
          if ((rules[i]->use == 1) && ((rules[i]->prio < rule->maxprio))) {
            /* remove rules with lower priority */
            rules[i]->use = 0;
            LOG4DEBUG(pL, "REMOVED RULE [%s] => prio", rules[i]->id);
          }
          i++;
        }
        count = i;
      }

      if (count > 1) {
        i = 0;
        while (i < rule->count) {
          if ((rules[i]->use == 1) && (rules[i]->route != NULL)) {
            rules[i]->use = 0;
            lastindex = i;
            LOG4WARN(pL, "checking multiple route actions [%s]", rules[i]->id);
          }
          i++;
        }
        rules[lastindex]->use = 1;
        LOG4WARN(pL, "route actions downselected to [%s]",
                 rules[lastindex]->id);
      }
    }
  }

  return;
}

/**
 *  @brief  create json object to be returned
 *
 *  @arg    s_input_t*, s_rulelist_t*, int*
 *  @return char*
 */

void *get_jsonresponse(s_rulelist_t *rulelist, char *next, size_t *lgth) {

  s_hdrlist_t *plist = NULL;
  s_hdr_t **phdr = NULL;
  s_rule_t **rules = NULL;

  cJSON *root = NULL;
  cJSON *hdr = NULL;
  cJSON *bdy = NULL;
  cJSON *hdrline = NULL;
  // cJSON *bdyline = NULL;

  size_t len = 0;

  char *presult = NULL;
  char *ptarget = NULL;
  char *pname = NULL;

  int i;
  int j;

  *lgth = 0;

  /* set default */
  ptarget = next;

  if (rulelist != NULL) {
    if (rulelist->rules != NULL) {
      rules = rulelist->rules;

      for (i = 0; i < rulelist->count; i++) {
        if (rules[i] != NULL) {
          if ((rules[i]->use == 1) && (rules[i]->valid)) {
            if (rules[i]->route != NULL) {
              ptarget = rules[i]->route;
              LOG4INFO(pL, "rule selected =>");
              LOG4INFO(pL, "...[%s: %s]", rules[i]->id, rules[i]->name);
              break;
            }
          }
        }
      }

      root = cJSON_CreateObject();
      hdr = cJSON_CreateArray();
      bdy = cJSON_CreateArray();

      cJSON_AddStringToObject(root, "target", ptarget);
      cJSON_AddNumberToObject(root, "statusCode", 200);
      cJSON_AddItemToObject(root, "additionalHeaders", hdr);
      cJSON_AddItemToObject(root, "additionalBodyParts", bdy);
      cJSON_AddNumberToObject(root, "tindex", 0);
      cJSON_AddNumberToObject(root, "tlabel", 0);

      for (i = 0; i < rulelist->count; i++) {
        if (rules[i] != NULL) {
          if ((rules[i]->addlst != NULL) && (rules[i]->use == 1) &&
              (rules[i]->valid)) {
            plist = rules[i]->addlst;
            if (plist->header != NULL) {
              phdr = plist->header;
              for (j = 0; j < plist->count; j++) {
                cJSON_AddItemToArray(hdr, hdrline = cJSON_CreateObject());
                len = strlen(phdr[j]->name) + 2;
                pname = (char *)malloc(len * sizeof(char));
                snprintf(pname, len, "%s:", phdr[j]->name);
                cJSON_AddStringToObject(hdrline, "name", pname);
                cJSON_AddStringToObject(hdrline, "value", phdr[j]->value);
                free(pname);
              }
            }
          }
        }
      }
      // cJSON_AddItemToArray(bdy, bdyline = cJSON_CreateObject());
      presult = cJSON_PrintUnformatted(root);
    } else {
      LOG4WARN(pL, "no valid rule found");
    }
  }

  if (presult == NULL) {
    LOG4ERROR(pL, "failed to create response, returning error");
    len = strlen(ERR_RESP) + strlen(ERR_DEFAULT) + 1;
    presult = (char *)malloc(len);
    snprintf(presult, len, ERR_RESP, ERR_DEFAULT);
  } else {
    LOG4DEBUG(pL, "JSON:\n[%s]\n", presult);
  }

  *lgth = strlen(presult);

  /* cleanup */
  cJSON_Delete(root);

  return presult;
}

/**
 *  @brief  main request handler (mongoose)
 *
 *  @arg    struct mg_connection*, struct http_message*
 *  @return void
 */

static void handle_req(struct mg_connection *nc, struct http_message *hm) {

  /* get rules and db file via user data */
  s_cfg_t *cfg = (s_cfg_t *)nc->mgr->user_data;

  s_input_t *request = (s_input_t *)malloc(sizeof(s_input_t));
  s_hdrlist_t *sipheader = NULL;
  s_rulelist_t *rulelist = NULL;

  request->ruri = NULL;
  request->next = NULL;
  request->shdr = NULL;

  char *res = NULL;
  char *shdr = NULL;

  size_t lgth;

  /* Get form variables */
  cJSON *jruri = NULL;
  cJSON *jshdr = NULL;
  cJSON *jnext = NULL;

  cJSON *jrequest = cJSON_Parse(hm->body.p);
  if (jrequest == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      LOG4ERROR(pL, "JSON error before: %s\n", error_ptr);
    }
  } else {
    jruri = cJSON_GetObjectItem(jrequest, "ruri");
    if (jruri != NULL) {
      if ((jruri->valuestring != NULL) && (strlen(jruri->valuestring) > 0)) {
        request->ruri =
            copy_string(jruri->valuestring, strlen(jruri->valuestring) + 1);
      }
    }

    jshdr = cJSON_GetObjectItem(jrequest, "request");
    if (jshdr != NULL) {
      if ((jshdr->valuestring != NULL) && (strlen(jshdr->valuestring) > 0)) {
        shdr = copy_string(jshdr->valuestring, strlen(jshdr->valuestring) + 1);
        res = (char *)base64_decode((unsigned char *)shdr, strlen(shdr), &lgth);
        if (res != NULL) {
          request->shdr = copy_string((char *)res, lgth);
        } else {
          LOG4WARN(pL, "base64 decoding returned empty message");
        }
        /* cleanup */
        if (shdr)
          free(shdr);
        if (res)
          free(res);
      }
    }

    jnext = cJSON_GetObjectItem(jrequest, "next");
    if (jnext != NULL) {
      if ((jnext->valuestring != NULL) && (strlen(jnext->valuestring) > 0)) {
        request->next =
            copy_string(jnext->valuestring, strlen(jnext->valuestring) + 1);
      }
    }

    cJSON_Delete(jrequest);
  }

  if (request->ruri) {
    LOG4DEBUG(pL, "ruri: [%s]", request->ruri);
  } else {
    LOG4WARN(pL, "request URI missing");
  }
  if (request->next) {
    LOG4DEBUG(pL, "next: [%s]", request->next);
  } else {
    LOG4WARN(pL, "next hop USI missing");
  }
  if (request->shdr) {
    LOG4DEBUG(pL, "shdr: [%zu] =>\n##\n%.*s ...\n##", lgth, 896, request->shdr);
  } else {
    LOG4WARN(pL, "SIP message header missing");
  }

  /* Send headers */
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  if (request->shdr) {
    sipheader = parse_list_crlf(request->shdr, SEP_HDR);
  } else {
    LOG4WARN(pL, "invalid SIP message");
  }

  LOG4INFO(pL, "request received =>");
  if (request->ruri != NULL) {
    LOG4INFO(pL, "...[ruri: %s]", request->ruri);
  }
  if (request->next != NULL) {
    LOG4INFO(pL, "...[next: %s]", request->next);
  }
  if ((res = get_listvalbyname(sipheader, FROM)) != NULL) {
    LOG4INFO(pL, "...[from: %s]", res);
  }
  if ((res = get_listvalbyname(sipheader, TO)) != NULL) {
    LOG4INFO(pL, "...[to:   %s]", res);
  }

  rulelist = parse_rule(cfg->rulefile);

  if (((sipheader != NULL) || (request->ruri) || (request->next)) &&
      (rulelist != NULL)) {
    LOG4DEBUG(pL, "VALIDATING === RULES ===");
    validate_rule(request, rulelist, sipheader, cfg->dbfile);
    LOG4DEBUG(pL, "SELECTING === RULE ===");
    select_rule(request, rulelist, sipheader);
    // print_rule(rulelist, FALSE);
    res = get_jsonresponse(rulelist, request->next, &lgth);
  } else {
    LOG4ERROR(pL, "sip header or rulelist missing");
    lgth = strlen(ERR_RESP) + strlen(ERR_DEFAULT) + 1;
    res = (char *)malloc(lgth);
    snprintf(res, lgth, ERR_RESP, ERR_DEFAULT);
  }

  if (rulelist != NULL) {
    LOG4DEBUG(pL, "DELETING === RULES ===");
    delete_rule(rulelist);
  }

  if (sipheader != NULL) {
    LOG4DEBUG(pL, "DELETING === SIP HEADER ===");
    delete_list(sipheader);
  }

  if (request->ruri)
    free(request->ruri);
  if (request->shdr)
    free(request->shdr);
  if (request->next)
    free(request->next);

  free(request);

  /* Compute the result and send it back as a JSON object */
  mg_printf_http_chunk(nc, "%.*s", lgth, res);
  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

  LOG4INFO(pL, "response sent =>");
  LOG4INFO(pL, "[%s]", res);

  /* cleanup */
  free(res);
}

/**
 *  @brief  defaul request handler (mongoose)
 *
 *  @arg    struct mg_connection*, struct http_message*
 *  @return void
 */

static void handle_default(struct mg_connection *nc, struct http_message *hm) {
  /* Send headers */
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  /* Compute the result and send it back as a JSON object */
  mg_printf_http_chunk(nc, ERR_RESP_STATIC);
  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

/**
 *  @brief  main event handler (mongoose)
 *
 *  @arg    struct mg_connection*, int, void*
 *  @return void
 */

void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {

  struct http_message *hm = (struct http_message *)ev_data;
  struct mbuf *io = &nc->recv_mbuf;

  switch (ev) {
  case MG_EV_HTTP_REQUEST:
    if (mg_vcmp(&hm->uri, "/api/v1/prf/req") == 0) {
      handle_req(nc, hm); /* Handle RESTful call */
    } else {
      handle_default(nc, hm);
    }
    break;
    mbuf_remove(io, io->len);
  default:
    break;
  }
}
