function fact(num)
    local total = 1
    for (var nn = num; nn > 1; nn=nn-1) do
        total = total * nn
    end
    return total
end

// basic factorial stress test

for (var i = 1000; i > 0; i=i-1) do
    for (var x = 100; x > 0; x=x-1) do
        print("The factorial of ", x, " is ", fact(x))
    end
end