-- 
-- tested on lua 5.2
--

local function get_proxy(tbl, name, DEBUG_PRINT) 

	local function val2str(v)
		return tostring(v)
	end

	local function args2str(...)
		local t = table.pack(...)
		local ret = '('
		for i = 1,t.n do
			if i > 1 then 
				ret = ret .. ', ' .. val2str(t[i])
			else
				ret = ret .. val2str(t[i])
			end
		end
		ret = ret .. ')'
		return ret
	end

	local mt = {
		__index = function(t, k)
			local target = tbl[k]
			if type(target) == "function" then
				return function(...)
					local args = args2str(...)
					local ret = table.pack(target(...))
					if #ret == 0 then
						if DEBUG_PRINT then 
							print(name .. "." .. k, args)
						end
					else
						if DEBUG_PRINT then 
							print(name .. "." .. k, args , "=", args2str(table.unpack(ret)))
						end
					end
					return table.unpack(ret)
				end
			end
			return target
		end,
	}

	local ret = {}
	setmetatable(ret, mt)
	return ret
end

function test()
	local foo = {
		hello = function (a,b,c)
			print("hello~", a, b, c)
			return "foo", "bar"
		end
	}

	print("[1]", foo.hello(1,2,3))
	
	local bar = get_proxy(foo, "foo", true)

	print("[2]", bar.hello(1,2,3))

end


test()
