function fact(i) 
    if (i == 0) then
        return 1;
    end
    return i*fact(i-1);
end

z = 1001;
while z-- > 0 do
	x = 0;
	while x < 10 do
        x++;
		//print("The factorial of ", x++, " is ", fact(x));
	end
end