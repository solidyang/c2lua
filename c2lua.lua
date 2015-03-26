
C2Lua = {};

function C2Lua.sizeof(struct_name)
	local size = 0;
	local size_func = C2Lua["Sizeof"..struct_name];
	if type(size_func) == "function" then
		size = size_func();
	end;
	return size;
end;

function C2Lua.getTypeOffset(obj, sVarName)
	local typ, off = 0;
	local func = C2Lua["Offset"..obj._sStructName_];
	if type(func) == "function" then
		typ, off = func(sVarName);
	end;
	
	off = off + obj._nStreamOffset_;
	return typ, off;
end;

function C2Lua.readData(obj, sVarName)
	local val;
	local typ, off = C2Lua.getTypeOffset(obj, sVarName);
	
	if type(typ) == "string" then
		val = C2Lua.locate(obj._Stream_, off, typ);
	elseif 0 == typ then
		val = obj._Stream_:GetSint8(off);
	elseif 1 == typ then
		val = obj._Stream_:GetUint8(off);
	elseif 2 == typ then
		val = obj._Stream_:GetSint16(off);
	elseif 3 == typ then
		val = obj._Stream_:GetUint16(off);
	elseif 4 == typ then
		val = obj._Stream_:GetSint32(off);
	elseif 5 == typ then
		val = obj._Stream_:GetUint32(off);
	elseif 6 == typ then
		val = obj._Stream_:GetFloat(off);
	elseif 7 == typ then
		val = obj._Stream_:GetDouble(off);
	elseif 8 == typ then
		val = obj._Stream_:GetString(off);
	elseif 9 == typ then
	end;
	return val;
end;

function C2Lua.writeData(obj, sVarName, val)
	local typ, off = C2Lua.getTypeOffset(obj, sVarName);
	if type(typ) == "string" then
		obj._Stream_:CopyFrom(off, val._Stream_, val._nStreamOffset_, C2Lua.sizeof(val._sStructName_));
	elseif 0 == typ then
		obj._Stream_:SetSint8(off, val);
	elseif 1 == typ then
		obj._Stream_:SetUint8(off, val);
	elseif 2 == typ then
		obj._Stream_:SetSint16(off, val);
	elseif 3 == typ then
		obj._Stream_:SetUint16(off, val);
	elseif 4 == typ then
		obj._Stream_:SetSint32(off, val);
	elseif 5 == typ then
		obj._Stream_:SetUint32(off, val);
	elseif 6 == typ then
		obj._Stream_:SetFloat(off, val);
	elseif 7 == typ then
		obj._Stream_:SetDouble(off, val);
	elseif 8 == typ then
		obj._Stream_:SetString(off, val);
	elseif 9 == typ then
	end;
end;

function C2Lua.locate(stream, off, struct_name)
	local t = {};
	t._Stream_ = stream;
	t._nStreamOffset_ = off;
	t._sStructName_ = struct_name;
	t.__index = C2Lua.readData;
	t.__newindex = C2Lua.writeData;
	
	setmetatable(t, t);
	return t;
end;