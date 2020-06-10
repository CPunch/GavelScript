var test = {}

test["hello"] = "world!"
test[1] = "hahahah"

local p = function() 
    // time to throw an objection!
    var t = 12
    t()
end

function printTbl(tbl)
    for (var z = 0; z < 10; ++z) do
        for (i, v in tbl) do
            print(z + ". - " + i + " : " + v)
            p()
        end
    end
    
    return function() return tbl[1] end
end

print(printTbl(test)())