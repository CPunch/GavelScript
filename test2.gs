function tblTest(tbl) 
	tbl[1] = "world!"
end

var t = {"hello", "dennis"}
tblTest(t)

for (i, v in t) do
	print(i, ": ", v)
end
