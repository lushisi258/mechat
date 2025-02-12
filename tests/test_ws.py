import asyncio
import websockets

async def handshake():
    uri = "ws://[::1]:2233"  # 替换[v6地址]为目标服务器的IPv6地址

    try:
        # 与WebSocket服务器建立连接
        async with websockets.connect(uri) as websocket:
            print(f"成功连接到 WebSocket 服务器：{uri}")
            # 握手成功后，可以发送和接收消息
            await websocket.send("Hello, Server!")
            response = await websocket.recv()
            print(f"服务器响应：{response}")

    except Exception as e:
        print(f"握手失败，错误信息：{str(e)}")

# 运行握手函数
asyncio.run(handshake())
