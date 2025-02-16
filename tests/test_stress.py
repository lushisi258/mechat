import websockets
import asyncio
import json
import time

async def send_messages(server_url, client_id, num_messages, message_delay):
    try:
        async with websockets.connect(server_url) as ws:
            for i in range(num_messages):
                message = {
                    "type": 1,
                    "sender": f"user{client_id}",
                    "receiver": "server",
                    "timestamp": int(time.time() * 1000),
                    "content": f"Message {i} from user{client_id}",
                    "meta": ""
                }
                await ws.send(json.dumps(message))
                response = await ws.recv()
                print(f"[Client {client_id}] Received: {response}")
                await asyncio.sleep(message_delay)
    except Exception as e:
        print(f"[❌] Client {client_id} error: {e}")

async def run(server_url, num_clients, num_messages, message_delay):
    tasks = [send_messages(server_url, i, num_messages, message_delay) for i in range(num_clients)]
    await asyncio.gather(*tasks)

if __name__ == "__main__":
    asyncio.run(run("ws://localhost:8080/ws", 10, 20, 0.1))
