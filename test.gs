var test = {}

//test[1337] = "le3t"
test["hello"] = "world!"
test[1] = "hahahah"

function printTbl(tbl)
    for (var z = 0; z < 10; ++z) do
        for (i, v in tbl) do
            if (i != 1337) then
                print(z + ". - " + i + " : " + v)
            else
                return function() return tbl[1] end // testing upvalues
            end
        end
    end
    
    return function() return tbl[1] end
end

print(printTbl(test)())