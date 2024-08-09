import idaapi

start = 0x000000000EB43A80
end = 0x000000000F274BE8

size = end - start

data = idaapi.get_bytes(start, size)

with open("dump.bin", "wb") as f:
    f.write(data)

print("done.")