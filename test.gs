var test = {}

//test[1337] = "le3t"
test["hello"] = "world!"
test[1] = "hahahah"

function printTbl(tbl)
    for (var z = 0; z < 10; z++) do
        for (i, v in tbl) do
            if (i != 1337) then
                print(i + " : " + v)
            else
                return true
            end
        end
    end

    return false // test failed
end

if printTbl(test) then
    print("YAY SCRIPT WORKS!!!")
end