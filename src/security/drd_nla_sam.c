#include "security/drd_nla_sam.h"

#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <winpr/ntlm.h>

#include "utils/drd_log.h"

struct _DrdNlaSamFile
{
    gchar *path;
};

/*
 * 功能：安全清零给定缓冲区。
 * 逻辑：若缓冲区有效则使用 volatile 指针逐字节写零，避免被编译器优化掉。
 * 参数：buffer 待清零缓冲区；len 长度。
 * 外部接口：无额外外部库调用。
 */
static void
drd_nla_memzero(gchar *buffer, gsize len)
{
    if (buffer == NULL || len == 0)
    {
        return;
    }

    volatile gchar *ptr = buffer;
    while (len-- > 0)
    {
        ptr[len] = 0;
    }
}

/*
 * 功能：按 SAM 文件格式拼装一条用户记录。
 * 逻辑：分配缓冲区后依次写入 username、分隔符和 NT 哈希，再追加换行。
 * 参数：username 用户名；nt_hash_hex 16 字节 NT 哈希的十六进制字符串。
 * 外部接口：GLib g_malloc0/g_stpcpy。
 */
static gchar *
drd_nla_sam_format_entry(const gchar *username, const gchar *nt_hash_hex)
{
    const gsize username_len = strlen(username);
    const gsize hash_len = strlen(nt_hash_hex);
    const gsize buffer_len = username_len + 3 + hash_len + 4;
    gchar *line = g_malloc0(buffer_len);

    g_stpcpy(line, username);
    g_stpcpy(line + strlen(line), ":::");
    g_stpcpy(line + strlen(line), nt_hash_hex);
    g_stpcpy(line + strlen(line), ":::\n");
    return line;
}

/*
 * 功能：将单条 SAM 记录写入文件并刷盘。
 * 逻辑：循环 write 写满所有字节，处理中断重试；写入完成后 fsync 保证落盘，最后清零内存缓存。
 * 参数：fd 打开的文件描述符；username 用户名；nt_hash_hex NT 哈希字符串；error 错误输出。
 * 外部接口：POSIX write/fsync；GLib g_set_error、g_io_error_from_errno；依赖 drd_nla_sam_format_entry 构造内容。
 */
static gboolean
drd_nla_sam_write_entry(int fd, const gchar *username, const gchar *nt_hash_hex, GError **error)
{
    g_autofree gchar *entry = drd_nla_sam_format_entry(username, nt_hash_hex);
    const gsize total = strlen(entry);
    gsize written = 0;

    while (written < total)
    {
        ssize_t ret = write(fd, entry + written, total - written);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            g_set_error(error,
                        G_IO_ERROR,
                        g_io_error_from_errno(errno),
                        "Failed to write SAM file: %s",
                        g_strerror(errno));
            return FALSE;
        }
        written += (gsize) ret;
    }

    if (fsync(fd) != 0)
    {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to flush SAM file: %s",
                    g_strerror(errno));
        return FALSE;
    }

    drd_nla_memzero(entry, total);

    return TRUE;
}

/*
 * 功能：确定 SAM 文件存放目录。
 * 逻辑：优先使用 XDG runtime 目录，否则回退到 /tmp 下的 drd 子目录。
 * 参数：无。
 * 外部接口：GLib g_get_user_runtime_dir/g_get_tmp_dir/g_build_filename。
 */
static gchar *
drd_nla_sam_default_dir(void)
{
    const gchar *runtime_dir = g_get_user_runtime_dir();
    if (runtime_dir != NULL)
    {
        return g_build_filename(runtime_dir, "drd", NULL);
    }
    return g_build_filename(g_get_tmp_dir(), "drd", NULL);
}

/*
 * 功能：创建包含 NLA 凭据的临时 SAM 文件。
 * 逻辑：确定目录并创建（0700）；使用 mkstemp 模板创建临时文件；写入用户名与 NT 哈希；成功则返回封装的 DrdNlaSamFile，失败时清理文件。
 * 参数：username 用户名；nt_hash_hex NT 哈希；error 错误输出。
 * 外部接口：GLib g_mkdir_with_parents/g_build_filename/g_mkstemp_full/g_unlink；POSIX write/close；内部 drd_nla_sam_write_entry。
 */
DrdNlaSamFile *
drd_nla_sam_file_new(const gchar *username, const gchar *nt_hash_hex, GError **error)
{
    g_return_val_if_fail(username != NULL && *username != '\0', NULL);
    g_return_val_if_fail(nt_hash_hex != NULL && *nt_hash_hex != '\0', NULL);

    g_autofree gchar *base_dir = drd_nla_sam_default_dir();
    if (g_mkdir_with_parents(base_dir, 0700) != 0 && errno != EEXIST)
    {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to create SAM directory '%s': %s",
                    base_dir,
                    g_strerror(errno));
        return NULL;
    }

    g_autofree gchar *template_path = g_build_filename(base_dir, "nla-sam-XXXXXX", NULL);
    int fd = g_mkstemp_full(template_path, O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0)
    {
        g_set_error(error,
                    G_IO_ERROR,
                    g_io_error_from_errno(errno),
                    "Failed to create SAM file: %s",
                    g_strerror(errno));
        return NULL;
    }

    if (!drd_nla_sam_write_entry(fd, username, nt_hash_hex, error))
    {
        close(fd);
        g_unlink(template_path);
        return NULL;
    }

    close(fd);

    DrdNlaSamFile *sam_file = g_new0(DrdNlaSamFile, 1);
    sam_file->path = g_strdup(template_path);
    return sam_file;
}

/*
 * 功能：获取 SAM 文件路径。
 * 逻辑：校验对象后返回路径字符串。
 * 参数：sam_file SAM 文件对象。
 * 外部接口：无额外外部库。
 */
const gchar *
drd_nla_sam_file_get_path(DrdNlaSamFile *sam_file)
{
    g_return_val_if_fail(sam_file != NULL, NULL);
    return sam_file->path;
}

/*
 * 功能：删除并释放 SAM 文件对象。
 * 逻辑：存在路径则先 unlink 文件，再释放路径与对象内存。
 * 参数：sam_file SAM 文件对象。
 * 外部接口：GLib g_unlink/g_free。
 */
void
drd_nla_sam_file_free(DrdNlaSamFile *sam_file)
{
    if (sam_file == NULL)
    {
        return;
    }

    if (sam_file->path != NULL)
    {
        g_unlink(sam_file->path);
    }

    g_free(sam_file->path);
    g_free(sam_file);
}

/*
 * 功能：将明文密码转换为 NTLM v1 哈希的十六进制串。
 * 逻辑：调用 WinPR NTOWFv1A 生成 16 字节哈希，再格式化为 32 字节十六进制字符串。
 * 参数：password 明文密码。
 * 外部接口：WinPR NTOWFv1A 计算哈希；GLib g_snprintf/g_malloc0；日志 DRD_LOG_WARNING。
 */
gchar *
drd_nla_sam_hash_password(const gchar *password)
{
    g_return_val_if_fail(password != NULL && *password != '\0', NULL);

    uint8_t hash[16];
    if (!NTOWFv1A((LPSTR) password, strlen(password), hash))
    {
        DRD_LOG_WARNING("Failed to initialize NT hash for NLA credentials");
        return NULL;
    }

    gchar *hex = g_malloc0(33);
    for (gsize i = 0; i < G_N_ELEMENTS(hash); i++)
    {
        g_snprintf(hex + (i * 2), 3, "%02" PRIx8, hash[i]);
    }
    drd_nla_memzero((gchar *) hash, sizeof(hash));

    return hex;
}
