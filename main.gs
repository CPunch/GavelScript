// forcing the factorial function to be a local means we don't have to ask the vm to lookup a global every time we want to call fact(). fact is always on the stack :)
local fact = function(num)
    local total = 1
    for (var i = num; i > 1; i=i-1) do
        total = total * i
    end
    return total
end

// basic factorial stress test, on cpunch's machine it takes ~1.90s
for (var i = 1000; i > 0; --i) do
    for (var x = 100; x > 0; --x) do
        // instead of concating the strings together in gavel, just pass the strings to print, where it will concat them on the console for you.
        // this saves performance as concating strings takes a lot of work (it has to run the extra instructions, intern the new string in the VM, possibly garbage collect, etc.)
        //print("The factorial of ", x, " is ", fact(x))
        fact(x)
    end
end