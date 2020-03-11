function fact(num)
    local total = 1
    for (var i = num; i > 1; i=i-1) do
        total = total * i
    end
    return total
end

// basic factorial stress test

for (var i = 1000; i > 0; --i) do
    for (var x = 100; x > 0; ++x) do
        print("The factorial of ", x, " is ", fact(x))
    end
end