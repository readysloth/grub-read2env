#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/dl.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/extcmd.h>
#include <grub/charset.h>

typedef int bool;

#define RAISE_EXCEPTION_IF_ANY(MSG, ...) \
  if(grub_errno != GRUB_ERR_NONE){ \
    if(MSG && MSG[0] != '\0'){ \
      grub_printf(MSG"\n"); \
    } \
    __VA_ARGS__; \
    return grub_errno; \
  }

#define THROW(ERR, MSG) grub_error(ERR, MSG); RAISE_EXCEPTION_IF_ANY("")
#define POSSIBLE_THROW(MSG, BODY, ...) BODY; RAISE_EXCEPTION_IF_ANY(MSG, __VA_ARGS__)


GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] = {
    {"path",  'p', 0, "path to file",                       NULL,      ARG_TYPE_FILE},
    {"set",   's', 0, "name of variable",                   "VARNAME", ARG_TYPE_STRING},
    {"utf16", 'u', 0, "is file utf16-encoded",              NULL,      ARG_TYPE_NONE},
    {"debug", 'd', 0, "print variable contents before set", NULL,      ARG_TYPE_NONE},
    {0, 0, 0, 0, 0, 0}
};

static grub_extcmd_t cmd;


static grub_uint8_t*
utf16_to_ascii(grub_uint8_t* dest, grub_uint8_t* src, grub_size_t size){
  for(grub_size_t i = 0, j = 0; j < size; j++){
    if(grub_isprint(src[j])){
      dest[i++] = src[j];
    }
  }
  return dest;
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
  grub_size_t buffer_size = 0;
  grub_uint8_t *read_buf = NULL;
  grub_uint8_t *env_buf = NULL;

  POSSIBLE_THROW("file open failed",
                 grub_file_t file = grub_file_open(path_to_file, GRUB_FILE_TYPE_NONE));
  buffer_size = file->size;
  if(!buffer_size){
    grub_printf("File is empty, environment variable wouldn't be set\n");
    return GRUB_ERR_NONE;
  }

  POSSIBLE_THROW("can't allocate memory for file",
                 read_buf = grub_calloc(buffer_size, sizeof(grub_uint8_t)),
                 grub_file_close(file));

  POSSIBLE_THROW("error occured in file read",
                 grub_ssize_t count = grub_file_read(file, read_buf, buffer_size),
                 grub_free(read_buf),
                 grub_file_close(file));

  if(count < 0 || (grub_size_t)count < buffer_size){
    THROW(GRUB_ERR_EOF, "file read ended in wrong size");
  }

  err = grub_file_close(file);
  if(err != GRUB_ERR_NONE){
    grub_free(env_buf);
    THROW(err, "error occured while closing the file");
  }

  if(state[2].set){
    env_buf = utf16_to_ascii(read_buf, read_buf, buffer_size);
  }
  else{
    env_buf = read_buf;
  }

  if(state[3].set){
    grub_printf("%s=\"%s\"\n", env_name, env_buf);
    grub_printf("xxd(%s)=\"", env_name);
    for(grub_size_t i = 0; i < buffer_size; i++){
      grub_printf("%02X", env_buf[i]);
    }
    grub_printf("\"\n");
  }

  err = grub_env_set(env_name, (char*)env_buf);
  grub_free(env_buf);
  THROW(err, "environment variable set failed");
  return GRUB_ERR_NONE;
}


GRUB_MOD_INIT(read)
{
  cmd = grub_register_extcmd("read2env",
                             grub_cmd_read2env,
                             0,
                             NULL,
                             "Set variable with file contents.",
                             options);
}

GRUB_MOD_FINI(read)
{
  grub_unregister_extcmd(cmd);
}
