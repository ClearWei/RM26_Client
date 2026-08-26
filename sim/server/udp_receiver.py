
import socket
import threading
import struct
from . import protocol
from . import crc

class UDPReceiver:
    def __init__(self, state_manager, host="0.0.0.0", port=3333):
        self.state_manager = state_manager
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.host, self.port))
        self.running = False
        print(f"UDP Receiver listening on {self.host}:{self.port}")

    def start(self):
        self.running = True
        threading.Thread(target=self._receive_loop, daemon=True).start()

    def stop(self):
        self.running = False

    def _receive_loop(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(1024)
                self._process_packet(data)
            except Exception as e:
                print(f"UDP Receive Error: {e}")

    def _process_packet(self, data):
        if len(data) < 9:  # 至少包含 7 字节帧头和 2 字节 CRC16
            return

        # 1. 校验帧起始字节
        if data[0] != protocol.SOF:
            return

        # 2. 校验帧头 CRC8。CRC8 位于索引 4，覆盖前 4 字节
        # （SOF、数据长度和序号），与 C++ 侧 Verify_CRC8_Check_Sum(data, 5) 保持一致。
        calc_crc8 = crc.get_crc8_check_sum(data[:4], 4, 0xff)
        if calc_crc8 != data[4]:
            print("CRC8 Failed")
            return

        # 3. 解析帧头：<BHBBH 依次为 SOF、长度、序号、CRC8 和 CmdID
        header = struct.unpack(protocol.HEADER_FMT, data[:7])
        data_len = header[1]
        cmd_id = header[4]

        # 4. 校验完整帧长度
        if len(data) < 7 + data_len + 2:
            print("Packet too short")
            return

        # 5. 校验整帧 CRC16，帧尾两字节按小端序保存接收值
        received_crc16 = data[-2] | (data[-1] << 8)
        calc_crc16 = crc.get_crc16_check_sum(data[:-2], len(data)-2, 0xffff)

        if calc_crc16 != received_crc16:
            print("CRC16 Failed")
            return

        # 6. 处理数据域
        payload = data[7:7+data_len]

        if cmd_id == protocol.CmdID.ROBOT_CONTROL:
            # <BB 依次为指令类型和目标 ID
            cmd_type, target_id = struct.unpack(protocol.ROBOT_CONTROL_FMT, payload)
            print(f"Received Robot Control: Type={cmd_type}, ID={target_id}")
            # 控制指令的状态更新仍由客户端处理，模拟器在这里保留接收日志。
            self.state_manager.log("UDP", f"Robot Control: Type={cmd_type}, ID={target_id}")

        elif cmd_id == protocol.CmdID.MAP_INTERACTION:
            # <ffB 依次为 x、y 和标记类型
            x, y, mark_type = struct.unpack(protocol.MAP_INTERACTION_FMT, payload)
            print(f"Received Map Interaction: X={x}, Y={y}, Type={mark_type}")
            self.state_manager.log("UDP", f"Map Mark: X={x:.2f}, Y={y:.2f}, Type={mark_type}")
