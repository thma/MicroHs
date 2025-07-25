{-# LANGUAGE ForeignFunctionInterface #-}

module MicroHs.Eval (
    MhsContext,
    withMhsContext,
    evalString,
    MhsError(..)
) where

import Foreign
import Foreign.C.String
import Foreign.C.Types
import Control.Exception
import qualified Data.ByteString as BS

-- Opaque context type
newtype MhsContext = MhsContext (Ptr ())

data MhsError = 
    MhsInitError String
  | MhsEvalError String
  deriving (Show)

instance Exception MhsError

-- Foreign imports
foreign import ccall "mhs_init_context"
    c_mhs_init_context :: IO (Ptr ())

foreign import ccall "mhs_free_context"
    c_mhs_free_context :: Ptr () -> IO ()

foreign import ccall "mhs_eval_string"
    c_mhs_eval_string :: Ptr () -> CString -> CSize -> Ptr CString -> Ptr CSize -> IO CInt

foreign import ccall "mhs_free_result"
    c_mhs_free_result :: CString -> IO ()

foreign import ccall "mhs_get_error"
    c_mhs_get_error :: Ptr () -> IO CString

-- High-level interface
withMhsContext :: (MhsContext -> IO a) -> IO a
withMhsContext action = do
    ctx_ptr <- c_mhs_init_context
    if ctx_ptr == nullPtr
        then throwIO $ MhsInitError "Failed to initialize MicroHs context"
        else do
            result <- action (MhsContext ctx_ptr) `finally` c_mhs_free_context ctx_ptr
            return result

evalString :: MhsContext -> String -> IO String
evalString (MhsContext ctx) expr = do
    withCStringLen expr $ \(expr_ptr, expr_len) -> do
        alloca $ \result_ptr -> do
            alloca $ \result_len_ptr -> do
                ret <- c_mhs_eval_string ctx expr_ptr (fromIntegral expr_len) result_ptr result_len_ptr
                if ret == 0
                    then do
                        result_cstr <- peek result_ptr
                        result_len <- peek result_len_ptr
                        result <- peekCStringLen (result_cstr, fromIntegral result_len)
                        c_mhs_free_result result_cstr
                        return result
                    else do
                        err_ptr <- c_mhs_get_error ctx
                        err_msg <- peekCString err_ptr
                        throwIO $ MhsEvalError err_msg