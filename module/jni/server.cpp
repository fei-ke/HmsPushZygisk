#include <sys/sendfile.h>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include "server.h"
#include "logging.h"

using namespace std;

static constexpr auto CONFIG_PATH = "/data/misc/hmspush/app.conf";

off_t sendFile(int remote_fd, const string &path) {
    auto in_fd = open(path.c_str(), O_RDONLY);
    if (in_fd < 0) {
        LOGE("Failed to open file %s: %d (%s)", path.c_str(), errno, strerror(errno));
        return -1;
    }

    auto size = lseek(in_fd, 0, SEEK_END);
    if (size < 0) {
        LOGI("Failed to get file size");
        close(in_fd);
        return -1;
    }
    lseek(in_fd, 0, SEEK_SET);

    // Send size first for buffer allocation
    int ret = write(remote_fd, &size, sizeof(size));
    if (ret < 0) {
        LOGI("Failed to send size");
        close(in_fd);
        return -1;
    }

    ret = sendfile(remote_fd, in_fd, nullptr, size);
    if (ret < 0) {
        LOGI("Failed to send data");
        close(in_fd);
        return -1;
    }

    close(in_fd);
    return size;
}

void Server::companion_handler(int remote_fd) {
    auto size = sendFile(remote_fd, (const string) CONFIG_PATH);
    LOGD("Sent module payload: %ld bytes", size);
}