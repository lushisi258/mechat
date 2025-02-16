import json
import asyncio
import test_connection
import test_message
import test_stress

# 读取配置文件
with open("/home/lushisi/projects/mechat/tests/config.json", "r") as f:
    config = json.load(f)

async def main():
    print("==== 开始自动化测试 ====")
    
    print("\n[1] 连接测试")
    await test_connection.run(config["server_url"])

    # print("\n[2] 消息格式测试")
    # await test_message.run(config["server_url"])

    # print("\n[3] 压力测试")
    # await test_stress.run(config["server_url"], config["num_clients"], config["num_messages"], config["message_delay"])

    print("\n==== 测试完成 ====")

if __name__ == "__main__":
    asyncio.run(main())
