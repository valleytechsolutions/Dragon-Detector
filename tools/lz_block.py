"""Deterministic original LZ4 block encoder for generated constant data."""
def pack(data):
    data=bytes(data)
    out=bytearray();latest={};i=anchor=0;n=len(data)
    def extra(value):
        while value>=255:out.append(255);value-=255
        out.append(value)
    while i+12<=n:
        key=data[i:i+4];old=latest.get(key,-1);latest[key]=i
        if old<0 or i-old>65535:i+=1;continue
        match=4
        while i+match<n-5 and data[old+match]==data[i+match]:match+=1
        literal=i-anchor;out.append((min(literal,15)<<4)|min(match-4,15))
        if literal>=15:extra(literal-15)
        out+=data[anchor:i];out+=(i-old).to_bytes(2,'little')
        if match-4>=15:extra(match-19)
        end=i+match
        for j in range(i+1,end):latest[data[j:j+4]]=j
        i=anchor=end
    literal=n-anchor;out.append(min(literal,15)<<4)
    if literal>=15:extra(literal-15)
    out+=data[anchor:];return bytes(out)
