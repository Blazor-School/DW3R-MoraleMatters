    bool HashExecutable(const wchar_t* path, char (&result)[65])
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        HANDLE file = INVALID_HANDLE_VALUE;
        bool success = false;
        do
        {
            if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
                break;
            if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
                break;
            file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                break;
            uint8_t buffer[65536];
            DWORD read = 0;
            bool readOk = true;
            for (;;)
            {
                if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr))
                {
                    readOk = false;
                    break;
                }
                if (!read)
                    break;
                if (BCryptHashData(hash, buffer, read, 0) < 0)
                {
                    readOk = false;
                    break;
                }
            }
            uint8_t digest[32];
            if (!readOk || BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0)
                break;
            const char hex[] = "0123456789abcdef";
            for (size_t index = 0; index < sizeof(digest); ++index)
            {
                result[index * 2] = hex[digest[index] >> 4];
                result[index * 2 + 1] = hex[digest[index] & 15];
            }
            result[64] = '\0';
            success = true;
        } while (false);
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        return success;
    }
