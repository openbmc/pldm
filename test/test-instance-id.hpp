
static constexpr auto pldmMaxInstanceIds = 32;
class TestWrapper
{
  public:
    TestWrapper()
    {
        static const char dbTmpl[] = "/tmp/db.XXXXXX";
        char dbName[sizeof(dbTmpl)] = {};

        ::strncpy(dbName, dbTmpl, sizeof(dbName));
        fd = ::mkstemp(dbName);
        if (fd == -1)
        {
            throw std::exception();
        }

        dbPath = std::filesystem::path(dbName);
        std::filesystem::resize_file(
            dbPath, (uintmax_t)(PLDM_MAX_TIDS)*pldmMaxInstanceIds);
    };
    ~TestWrapper()
    {
        std::filesystem::remove(dbPath);
        ::close(fd);
    };

  protected:
    std::filesystem::path dbPath;

  private:
    int fd;
};
