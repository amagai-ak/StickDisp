#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
from datetime import datetime
import argparse
import os
import time
import serial

debuglevel = 0

class StickDispCtrl:
    def __init__(self):
        self.led = False
        self.ser = None
        self.rxstr = ''
        self.rxstat = 0
        self.callback = None

    def open(self, port):
        self.ser = serial.Serial(port, 115200, timeout=0.1)
        self.ser.write(b'\r\n')

    def close(self):
        self.ser.close()

    def set_led(self, ledon):
        self.led = ledon
        if ledon == True:
            self.ser.write(b'$l1\r\n')
        else:
            self.ser.write(b'$l0\r\n')

    def get_led(self):
        return self.led

    def set_text(self, n, msg):
        if n == 0 or n == 1:
            cmdstr = '$t' + str(n) + msg + '\r\n'
            self.ser.write(cmdstr.encode())

    def power_off(self):
        self.ser.write(b'$p\r\n')

    # 現在時刻設定する
    # 時刻の変わり目を待って送信する
    def set_rtc(self):
        now = datetime.now()
        pre_dtstr = now.strftime("%Y%m%d%H%M%S")

        while True:
            now = datetime.now()
            dtstr = now.strftime("%Y%m%d%H%M%S")
            if dtstr != pre_dtstr:
                break;
            else:
                time.sleep(0.01)
        # print(dtstr)
        rtcstr = '$w' + dtstr + '\r\n'
        self.ser.write(rtcstr.encode())

    def get_version(self):
        self.ser.write(b'$V\r\n')
        tout = 20
        while tout > 0:
            rtnstr = self.poll()
            if rtnstr[0] == 'V':
                return rtnstr[1:]
            else:
                tout = tout - 1
        return None

    # 現在時刻の読み出し．1.5秒でタイムアウト．
    # タイムアウトしたらNoneを返す．
    # 読めた場合は (dy, mo, dt, hh, mm, ss)
    def read_rtc(self):
        self.ser.write(b'$R\r\n')
        tout = 20
        while tout > 0:
            rtnstr = self.poll()
            # print(rtnstr)
            # R20220619220230
            if len(rtnstr) == 15 and rtnstr[0] == 'R':
                try:
                    dy = int(rtnstr[1:5])
                    dm = int(rtnstr[5:7])
                    dd = int(rtnstr[7:9])
                    hh = int(rtnstr[9:11])
                    mm = int(rtnstr[11:13])
                    ss = int(rtnstr[13:15])
                except:
                    return None
                # validation
                if dy <= 2000 or dy >= 3000 or \
                    dm < 0 or dm > 12 or \
                    dd < 0 or dd > 31 or \
                    hh < 0 or hh > 24 or \
                    mm < 0 or mm > 59 or \
                    ss < 0 or ss > 59:
                    return None
                return (dy, dm, dd, hh, mm, ss)
            else:
                tout = tout - 1
        return None

    def set_callback(self, callback):
        self.callback = callback

    def get_rxstr(self):
        return self.rxstr

    def set_label(self, n, lbl):
        cmdstr = '$v' + str(n) + '0' + lbl + '\r\n'
        self.ser.write(cmdstr.encode())

    def set_value(self, n, lbl):
        cmdstr = '$v' + str(n) + '1' + lbl + '\r\n'
        self.ser.write(cmdstr.encode())

    def set_screen(self, n):
        if n >= 0 and n < 3:
            cmdstr = '$s' + str(n) + '\r\n'
            self.ser.write(cmdstr.encode())

    def poll(self):
        rtnstr = ''
        while True:
            c = self.ser.read(1)
            if len(c) > 0:
                if self.rxstat == 0:
                    if c == b'$':
                        self.rxstat = 1
                else:
                    if c == b'\r':
                        # print(self.rxstr)
                        self.rxstat = 0
                        rtnstr = self.rxstr
                        if self.callback != None:
                            self.callback(self)
                        self.rxstr = ''
                    else:
                        self.rxstr = self.rxstr + c.decode()
                        if len(self.rxstr) > 32:
                            self.rxstr = ''
                            self.rxstat = 0
            else:
                break
        return rtnstr


# CPUの温度を取得する
def get_cputemp():
    with open('/sys/class/thermal/thermal_zone0/temp') as f:
        return float(f.read()) / 1000

# ファイルシステムの空き容量を取得する (GB)
def get_freespace():
    st = os.statvfs('/')
    return (st.f_bavail * st.f_frsize) / (1024*1024*1024)

# RTCの時刻を表示する
def show_rtc(disp):
    # (dy, mo, dt, hh, mm, ss)
    rtc = disp.read_rtc()
    if rtc != None:
        # print(rtc)
        sys.stdout.write('%04d-%02d-%02d %02d:%02d:%02d\n' % rtc)
    else:
        sys.stderr.write("RTC read timeout\n")
        return False
    return True


# ボタンのステータス
# 'bxA' : 画面xでボタンAが押された
# 'bxB' : 画面xでボタンBが押された

# ボタンが押されたときに呼び出されるコールバック関数．
# 受信文字列は，先頭の$は除去されている．
# ボタンに応じて何か処理をしたい場合にはここに記述する．
def disp_callback(disp):
    global debuglevel
    s = disp.get_rxstr()
    if debuglevel > 0:
        print("Callback :" + s)
    if s == 'b2A':
        # print("Shutdown")
        # disp.power_off()
        disp.close()
        # os.system('shutdown -h now &')
        sys.exit(1)
    if s == 'b0A':
        disp.set_led(not disp.get_led())


def main(args):
    global debuglevel

    debuglevel = args.debug

    disp = StickDispCtrl()
    try:
        disp.open(args.port)
    except:
        sys.stderr.write("Failed to open %s\n" % (args.port))
        return 1
    
    # -r オプションが指定されていた場合はRTCの読み出しのみで終了する
    if args.rtc == True:
        show_rtc(disp)
        disp.close()
        return 0

    # -v オプションが指定されていた場合はバージョン情報の読み出しのみで終了する
    if args.version == True:
        vstr = disp.get_version()
        if vstr != None:
            print(vstr)
            disp.close()
            return 0
        else:
            disp.close()
            return 1

    # -w オプションが指定されていた場合はRTCに現在時刻を設定して終了する
    if args.write_rtc == True:
        disp.set_rtc()
        return 0

    disp.set_led(False)
    disp.set_rtc()
    if args.text1 != None:
        disp.set_text(0, args.text1)
    if args.text2 != None:
        disp.set_text(1, args.text2)

    disp.set_label(0, 'Temp')
    disp.set_label(1, 'Free')
    disp.set_label(2, 'Load')

    # 3行目は使わない
    disp.set_label(3, '')
    disp.set_value(3, '')

    disp.set_callback(disp_callback)

    t_old = int(time.time())

    # 100ms周期のループ
    while True:
        # poll()はtimeout=0.1秒で抜ける
        s = disp.poll()
        t = time.time()
        t_now = int(t)
        t_f = t - t_now
        # 60秒に1回rtcを更新する
        if args.update_rtc and t_f > 0.8 and t_f < 0.95 and t_now % 60 == 0:
            if debuglevel > 0:
                print("RTC Updated")
            disp.set_rtc()
        # 毎秒の処理
        if t_old != t_now:
            t_old = t_now
            t = get_cputemp()
            fs = get_freespace()
            lavg = os.getloadavg()
            disp.set_value(0, '%3.1f' % (t))
            disp.set_value(1, '%3.1f GiB' % (fs))
            disp.set_value(2, '%3.1f' % (lavg[0]))

    disp.close()
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', help='comm port', type=str, default='/dev/ttyUSB0')
    parser.add_argument('--text1', help='Text to display on line1', type=str, default=None)
    parser.add_argument('--text2', help='Text to display on line2', type=str, default=None)
    parser.add_argument('-r', '--rtc', help='Read RTC and exit', action='store_true')
    parser.add_argument('-w', '--write_rtc', help='Write current system time to RTC and exit', action='store_true')
    parser.add_argument('-u', '--update_rtc', help='Update RTC every minute', action='store_true')
    parser.add_argument('-v', '--version', help='Read version info', action='store_true')
    parser.add_argument('-d', '--debug', help='debug mode', type=int, default=0)
    args = parser.parse_args()
    sys.exit( main(args) )
