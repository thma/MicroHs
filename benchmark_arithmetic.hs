module Main where

-- Fibonacci using arithmetic operations
fib :: Int -> Int
fib 0 = 0
fib 1 = 1
fib n = fib (n - 1) + fib (n - 2)

-- Sum of squares
sumSquares :: Int -> Int
sumSquares 0 = 0
sumSquares n = n * n + sumSquares (n - 1)

-- Factorial
fact :: Int -> Int
fact 0 = 1
fact n = n * fact (n - 1)

-- Heavy arithmetic computation
compute :: Int -> Int
compute n = 
    let a = sumSquares n
        b = fact (n `div` 3)
        c = fib (n `div` 2)
    in (a + b) * c

main :: IO ()
main = do
    print (compute 20)
    print (compute 22)
    print (compute 24)