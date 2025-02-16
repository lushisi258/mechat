import websockets
import asyncio
import json
import time

async def run(server_url):
    try:
        async with websockets.connect(server_url) as ws:
            message = {
                "type": 1,  
                "sender": "user1",
                "receiver": "user2",
                "timestamp": int(time.time() * 1000),
                "content": "Hello, world!",
                "meta": ""
            }
            await ws.send(json.dumps(message))
            response = await ws.recv()
            response_data = json.loads(response)

            if response_data["type"] == 1 and response_data["content"] == "Hello, world!":
                print("[✅] 消息格式正确")
            else:
                print("[❌] 消息格式错误")
    except Exception as e:
        print(f"[❌] 消息测试失败: {e}")

if __name__ == "__main__":
    asyncio.run(run("ws://localhost:8080/ws"))
