local wow = "OK"
function test()
    local yay = "ok"
    function ed()
        yay = "no!"
        wow = "BRUH"
    end

    ed()
    return yay
end

print(test(), "_" + wow)