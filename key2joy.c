#include <linux/uinput.h>
#include <linux/joystick.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

void emit(int fd, u_int16_t type, u_int16_t code, int32_t val)
{
    struct input_event ie;

    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    if (write(fd, &ie, sizeof(ie)) != sizeof(ie)) {
        perror("emit button");
    }
}

int32_t pressed[KEY_CNT] = {0};

void readkeys(int fd2) {
    struct input_event ie;
    while (read(fd2, &ie, sizeof(ie)) == sizeof(ie)) {
        if (ie.type == EV_KEY && ie.code <= KEY_CNT) {
            printf("%d: %d\n", ie.code, ie.value); fflush(stdout);
            pressed[ie.code] = ie.value;
        }
    }
}

int main(int argc, char **argv)
{
    int ret = -1;
    /*setvbuf(stdin, NULL, _IONBF, 0);*/
    /*setvbuf(stdout, NULL, _IONBF, 0);*/

    if (argc <= 1) {
        fprintf(stderr, "Usage: sudo %s /dev/input/event*\n", argv[0]);
        // To find accurate event id, use: cat /proc/bus/input/devices
        // e.g. my keyboard is event5
        //I: Bus=0003 Vendor=258a Product=1006 Version=0111
        //N: Name="Gaming KB  Gaming KB "
        //P: Phys=usb-0000:00:14.0-1/input0
        //S: Sysfs=/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.0/0003:258A:1006.0001/input/input5
        //U: Uniq=
        //H: Handlers=sysrq kbd leds event5
        //B: PROP=0
        //B: EV=120013
        //B: KEY=1000000000007 ff9f207ac14057ff febeffdfffefffff fffffffffffffffe
        //B: MSC=10
        //B: LED=7
        goto out;
    }

    fd_set fds;
    int maxfdp = 0;
    FD_ZERO(&fds);

    for (int i = 1; i < argc; i++) {
        int fd2 = open(argv[i], O_RDONLY | O_NONBLOCK);
        if (fd2 < 0) {
            perror(argv[i]);
            fprintf(stderr, "Hint: sudo %s\n", argv[0]);
            return -1;
        }
        FD_SET(fd2, &fds);
        if (fd2 > maxfdp) {
            maxfdp = fd2;
        }
    }

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("/dev/uinput");
        fprintf(stderr, "Hint: sudo modprobe uinput\n");
        goto out_close_fds;
    }

    struct uinput_user_dev dev;
    memset(&dev, 0, sizeof(dev));
    dev.id.bustype = BUS_USB;
    dev.id.vendor = 0x054c;
    dev.id.product = 0x099c;
    strcpy(dev.name, "Fake XBox Controller");

    ioctl(fd, UI_SET_PHYS, "usb-Madcatz_Saitek_Pro_Flight_X-55_Rhino_Stick_G0000090-event-joystick");

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    /*ioctl(fd, UI_SET_EVBIT, EV_MSC);*/
    /*ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);*/

    u_int32_t keys[] = {
        /* update this if you adds more custom buttons */
        BTN_A, BTN_B, BTN_X, BTN_Y,
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        ioctl(fd, UI_SET_KEYBIT, keys[i]);
    }

    u_int32_t abss[] = {
        /* update this if you adds more custom axes */
        ABS_X, ABS_Y, ABS_RX, ABS_RY,
    };
    for (size_t i = 0; i < sizeof(abss) / sizeof(abss[0]); i++) {
        ioctl(fd, UI_SET_ABSBIT, abss[i]);
    }

    if (write(fd, &dev, sizeof(dev)) < 0) {
        perror("write uinput_user_dev");
        goto out_close_fd;
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        goto out_close_fd;
    }

    sleep(1);

    while (1) {
        fd_set fds2 = fds;
        int res = select(maxfdp, &fds2, NULL, NULL, NULL);
        if (res == -1) {
            break;
        } else if (res == 0) {
            continue;
        }

        for (int i = 0; i < maxfdp; i++) {
            if (FD_ISSET(i, &fds2)) {
                readkeys(i);
            }
        }

        /*flag = (flag + 1) % 10;*/
        /*int32_t absvals[] = {-!!flag * 32767, -32767, 0, 32767, !!flag * 32767};*/
        static const int32_t absvals[] = {-32767, -32767, 0, 32767, 32767};

        /* may edit your custom key mappings manually here */
        emit(fd, EV_ABS, ABS_X, absvals[2 + pressed[KEY_D] - pressed[KEY_A]]);
        emit(fd, EV_ABS, ABS_Y, absvals[2 + pressed[KEY_S] - pressed[KEY_W]]);
        emit(fd, EV_ABS, ABS_RX, absvals[2 + pressed[KEY_L] - pressed[KEY_J]]);
        emit(fd, EV_ABS, ABS_RY, absvals[2 + pressed[KEY_K] - pressed[KEY_I]]);
        emit(fd, EV_KEY, BTN_A, pressed[KEY_SPACE]);
        emit(fd, EV_KEY, BTN_B, pressed[KEY_LEFTCTRL]);
        emit(fd, EV_KEY, BTN_X, pressed[KEY_E]);
        emit(fd, EV_KEY, BTN_Y, pressed[KEY_Q]);
        /*emit(fd, EV_KEY, BTN_BACK, pressed[KEY_TAB]);*/
        /*emit(fd, EV_KEY, BTN_START, pressed[KEY_P]);*/
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }

    ret = 0;
out_close_fd:
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
out_close_fds:
    for (int i = 0; i < maxfdp; i++) {
        if (FD_ISSET(i, &fds)) {
            close(i);
        }
    }
out:
    return ret;
}
