import websockets
import asyncio

async def run(server_url):
    try:
        async with websockets.connect(server_url) as ws:
            print("[✅] WebSocket 连接成功")
    except Exception as e:
        print(f"[❌] WebSocket 连接失败: {e}")

if __name__ == "__main__":
    asyncio.run(run("ws://localhost:8080/ws"))
