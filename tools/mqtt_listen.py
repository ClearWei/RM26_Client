#!/usr/bin/env python3
"""监听 RoboMaster 引擎发布的 MQTT 消息。"""

import argparse
import json
import os
import socket
import sys
import time
from datetime import datetime

TOPIC = "#"

received_count = 0


def on_connect(client, userdata, flags, rc, properties=None):
    rc_int = rc.value if hasattr(rc, "value") else rc
    codes = {0: "OK", 128: "Unspecified error", 133: "Client ID not valid",
             134: "Bad username/password", 135: "Not authorized", 136: "Server unavailable"}
    msg = codes.get(rc_int, f"Unknown({rc_int})")
    print(f"[{ts()}] connect → {msg} (rc={rc_int})")
    if rc_int == 0:
        client.subscribe(TOPIC)
        print(f"[{ts()}] Subscribed '{TOPIC}' — waiting for messages...")


def on_disconnect(client, userdata, flags, rc, properties=None):
    rc_int = rc.value if hasattr(rc, "value") else rc
    print(f"[{ts()}] Disconnected (rc={rc_int})")


def on_message(client, userdata, msg):
    global received_count
    received_count += 1
    try:
        payload = msg.payload.decode("utf-8", errors="replace")
    except Exception:
        payload = str(msg.payload)
    try:
        obj = json.loads(payload)
        payload_fmt = json.dumps(obj, indent=2, ensure_ascii=False)
    except (json.JSONDecodeError, TypeError):
        payload_fmt = payload
    print(f"\n[{ts()}] #{received_count} topic={msg.topic}")
    print(f"  qos={msg.qos} retain={msg.retain}")
    print(f"  payload:\n{payload_fmt}")


def ts():
    return datetime.now().strftime("%H:%M:%S")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description="监听赛事引擎 MQTT 消息")
    parser.add_argument(
        "--broker",
        default=os.environ.get("RM_MQTT_BROKER"),
        help="MQTT broker 主机名或 IP，也可通过 RM_MQTT_BROKER 设置",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=os.environ.get("RM_MQTT_PORT", "3333"),
        help="MQTT 端口，默认 3333；也可通过 RM_MQTT_PORT 设置",
    )
    parser.add_argument(
        "--topic",
        default=os.environ.get("RM_MQTT_TOPIC", "#"),
        help="订阅主题，默认 #；也可通过 RM_MQTT_TOPIC 设置",
    )
    parser.add_argument(
        "--client-id",
        help="本次监听使用的 MQTT Client ID，必须由操作者显式指定",
    )
    parser.add_argument(
        "--allow-live-connect",
        action="store_true",
        help="确认允许连接现场 broker；同 Client ID 可能挤掉在线客户端",
    )
    args = parser.parse_args(argv)

    if not args.broker:
        parser.error("未指定 broker，请使用 --broker 或设置 RM_MQTT_BROKER")
    if "://" in args.broker:
        parser.error("broker 只填写主机名或 IP，不要包含协议前缀")
    if not 1 <= args.port <= 65535:
        parser.error("端口必须在 1 到 65535 之间")
    if not args.topic:
        parser.error("订阅主题不能为空")
    if not args.client_id:
        parser.error("未指定 Client ID，请使用 --client-id")
    if "\0" in args.client_id:
        parser.error("Client ID 不能包含空字符")
    if not args.allow_live_connect:
        parser.error(
            "连接现场 broker 可能挤掉相同 Client ID 的在线客户端；"
            "确认现场安全后再加 --allow-live-connect"
        )
    return args


def run_listener(args, mqtt):
    global TOPIC
    TOPIC = args.topic
    broker = args.broker
    port = args.port

    print(f"[{ts()}] MQTT listener — {broker}:{port}")

    # 先确认端口可达，现场网络未就绪时不反复创建 MQTT 客户端。
    print(f"[{ts()}] Waiting for {broker}:{port}...")
    for i in range(60):
        try:
            s = socket.create_connection((broker, port), timeout=2)
            s.close()
            print(f"[{ts()}] Port {broker}:{port} OPEN")
            break
        except (socket.timeout, ConnectionRefusedError, OSError):
            if i % 5 == 0:
                print(f"[{ts()}] ...waiting ({i*2}s)")
            time.sleep(2)
    else:
        print(f"[{ts()}] Timeout: port never opened")
        return 1

    client_id = args.client_id
    print(f"[{ts()}] Trying client_id={client_id} (v3.1.1)...")
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=client_id,
        protocol=mqtt.MQTTv311,
    )
    client.on_message = on_message
    client.on_disconnect = on_disconnect

    connected = {"ok": False}

    def on_conn(active_client, userdata, flags, rc, props=None):
        rc_int = rc.value if hasattr(rc, "value") else rc
        if rc_int == 0:
            userdata["ok"] = True
            active_client.subscribe(TOPIC)

    client.on_connect = on_conn
    client.user_data_set(connected)

    try:
        client.connect(broker, port, keepalive=60)
        t0 = time.time()
        while time.time() - t0 < 2:
            client.loop(timeout=0.1)
            if connected["ok"]:
                print(f"[{ts()}] CONNECTED client_id={client_id} — listening...")
                client.on_connect = on_connect
                try:
                    client.loop_forever()
                except KeyboardInterrupt:
                    print(f"\n[{ts()}] Stopped. Received {received_count} msgs.")
                finally:
                    client.disconnect()
                return 0
        client.disconnect()
        print(f"[{ts()}] 连接被 broker 拒绝或在超时内未完成。", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"[{ts()}] 连接失败: {exc}", file=sys.stderr)
        return 1


def main(argv=None):
    args = parse_args(argv)

    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        print("错误：未安装 paho-mqtt，请先运行 pip3 install paho-mqtt。", file=sys.stderr)
        return 1

    return run_listener(args, mqtt)


if __name__ == "__main__":
    sys.exit(main())
