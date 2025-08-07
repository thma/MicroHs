module Main where

-- Naive Fibonacci - a classic benchmark for arithmetic operations
nfib :: Int -> Int
nfib n = if n < 2 then 1 else nfib (n - 1) + nfib (n - 2) + 1

main :: IO ()
main = do
    -- Test with different values
    print (nfib 20)  -- Should be 21891
    print (nfib 25)  -- Should be 242785
    print (nfib 30)  -- Should be 2692537