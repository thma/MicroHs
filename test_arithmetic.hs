module Main where

-- Simple arithmetic test that uses ADD, SUB, MUL combinators
compute :: Int -> Int -> Int
compute x y = (x + y) * (x - y)

main :: IO ()
main = do
  let result1 = compute 10 3   -- Should be 91 (13 * 7)
  let result2 = compute 20 5   -- Should be 375 (25 * 15)
  let result3 = compute 7 2    -- Should be 45 (9 * 5)
  print result1
  print result2  
  print result3
  print (1 + 2 + 3 + 4 + 5)  -- Test repeated addition