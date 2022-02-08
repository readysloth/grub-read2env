#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/dl.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/extcmd.h>

typedef int bool;

#define RAISE_EXCEPTION_IF_ANY(MSG) \
  if(grub_errno != GRUB_ERR_NONE){ \
    if(MSG || MSG[0] != '\0'){ \
      grub_printf(MSG"\n"); \
    } \
    else{ \
      grub_print_error(); \
    } \
    return grub_errno; \
  }

#define THROW(ERR, MSG) grub_error(ERR, MSG); RAISE_EXCEPTION_IF_ANY("")
#define POSSIBLE_THROW(MSG, BODY) BODY; RAISE_EXCEPTION_IF_ANY(MSG)


GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] = {
    {"path",  'p', 0, "path to file",           NULL,      ARG_TYPE_FILE},
    {"set",   's', 0, "name of variable",       "VARNAME", ARG_TYPE_STRING},
    {"utf16", 'u', 0, "is file utf16-encoded",  NULL,      ARG_TYPE_NONE},
    {0, 0, 0, 0, 0, 0}
};

static grub_extcmd_t cmd;

static grub_err_t
utf16_to_ascii(grub_uint8_t *data, grub_size_t size){
  grub_uint8_t *data_start = data;

  grub_uint16_t BOM = *(grub_uint32_t*)(data);
  bool big_endian = BOM == 0xFEFF;
  bool little_endian = BOM == 0xFFFE;
  bool is_utf16 = big_endian || little_endian;

  if(!is_utf16){
    return THROW(GRUB_ERR_BAD_ARGUMENT, "data is not utf16-encoded");
  }

  grub_uint16_t *utf_16_data = (grub_uint16_t*)(data + sizeof(BOM));
  for(grub_size_t i = 0; i < size; i++){
    int char_index = little_endian ? 1 : 0;
    data_start[i] = ((grub_uint8_t*)utf_16_data++)[char_index];
  }
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_read2env(grub_extcmd_context_t ctxt,
                  int argc __attribute__((unused)),
                  char **args __attribute__((unused))){
  grub_err_t err = GRUB_ERR_NONE;
  struct grub_arg_list *state = ctxt->state;
  if(!state[0].set || !state[1].set){
    THROW(GRUB_ERR_BAD_ARGUMENT, "--path and --set should be set");
  }
  char *path_to_file = state[0].arg;
  char *env_name = state[1].arg;
  grub_uint8_t *env_buf = NULL;

  POSSIBLE_THROW("file open failed",
                 grub_file_t file = grub_file_open(path_to_file, GRUB_FILE_TYPE_NONE));
  grub_size_t buffer_size = file->size;

  POSSIBLE_THROW("can't allocate memory for file",
                 env_buf = grub_malloc(buffer_size));
  POSSIBLE_THROW("error occured in file read",
                 grub_ssize_t count = grub_file_read(file, env_buf, file->size));

  if(!count){
    THROW(GRUB_ERR_EOF, "0 bytes read from file");
  }

  err = grub_file_close(file);
  if(err != GRUB_ERR_NONE){
    THROW(err, "error occured while closing the file");
  }

  if(state[2].set){
    utf16_to_ascii(env_buf, buffer_size);
  }

  err = grub_env_set(env_name, (char*)env_buf);
  THROW(err, "environment variable set failed");
  grub_free(env_buf);
  return GRUB_ERR_NONE;
}


GRUB_MOD_INIT(read)
{
  cmd = grub_register_extcmd("read2env",
                             grub_cmd_read2env,
                             0,
                             NULL,
                             "Sets variable with file contents.",
                             options);
}

GRUB_MOD_FINI(read)
{
  grub_unregister_extcmd(cmd);
}
