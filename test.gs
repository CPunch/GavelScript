function test()
    local yay = "ok"
    function ed()
        yay = "no!"
    end

    ed()
    return yay
end

print(test())