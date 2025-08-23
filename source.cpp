#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <codecvt>
#include <memory>

class c_memory {
public:
    c_memory( const std::string& process_name ) {
        m_pid = get_pid( process_name );
        if ( m_pid == 0 ) {
            std::cout << "failed to find process\n";
            return;
        }

        m_process_handle = OpenProcess( PROCESS_VM_READ | PROCESS_QUERY_INFORMATION , FALSE , m_pid );
        if ( !m_process_handle ) {
            std::cout << "failed to open process\n";
            return;
        }

        m_base_address = get_module_base( process_name );
    }

    uintptr_t resolve_pointer( uintptr_t base , const std::vector<uintptr_t>& offsets ) {
        uintptr_t addr = base;
        for ( size_t i = 0; i < offsets.size( ); i++ ) {
            if ( !read_memory( addr , addr ) )
                return 0;
            addr += offsets [ i ];
        }
        return addr;
    }

    bool read_memory( uintptr_t address , uintptr_t& out_value ) {
        SIZE_T bytes_read;
        return ReadProcessMemory( m_process_handle , ( LPCVOID ) address , &out_value , sizeof( out_value ) , &bytes_read );
    }

    std::string read_string( uintptr_t address , size_t max_length , bool unicode , bool zero_terminate ) {
        if ( unicode ) {
            std::vector<wchar_t> buffer( max_length + 1 , 0 );
            SIZE_T bytes_read;
            if ( ReadProcessMemory( m_process_handle , ( LPCVOID ) address , buffer.data( ) , buffer.size( ) * sizeof( wchar_t ) , &bytes_read ) ) {
                std::wstring ws( buffer.data( ) );
                return std::wstring_convert<std::codecvt_utf8<wchar_t>>( ).to_bytes( ws );
            }
        }
        else {
            std::vector<char> buffer( max_length + 1 , 0 );
            SIZE_T bytes_read;
            if ( ReadProcessMemory( m_process_handle , ( LPCVOID ) address , buffer.data( ) , buffer.size( ) , &bytes_read ) ) {
                if ( zero_terminate )
                    return std::string( buffer.data( ) );
                else
                    return std::string( buffer.begin( ) , buffer.begin( ) + max_length );
            }
        }
        return {};
    }

    uintptr_t base_address( ) const { return m_base_address; }
    bool valid( ) const { return m_process_handle != nullptr; }

private:
    DWORD m_pid {};
    HANDLE m_process_handle {};
    uintptr_t m_base_address {};

    DWORD get_pid( const std::string& process_name ) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof( PROCESSENTRY32W );

        HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS , 0 );
        if ( snapshot == INVALID_HANDLE_VALUE )
            return 0;

        std::wstring wide_name( process_name.begin( ) , process_name.end( ) );

        if ( Process32FirstW( snapshot , &entry ) ) {
            do {
                if ( _wcsicmp( entry.szExeFile , wide_name.c_str( ) ) == 0 ) {
                    DWORD pid = entry.th32ProcessID;
                    CloseHandle( snapshot );
                    return pid;
                }
            } while ( Process32NextW( snapshot , &entry ) );
        }

        CloseHandle( snapshot );
        return 0;
    }

    uintptr_t get_module_base( const std::string& module_name ) {
        MODULEENTRY32W entry;
        entry.dwSize = sizeof( MODULEENTRY32W );

        HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32 , m_pid );
        if ( snapshot == INVALID_HANDLE_VALUE )
            return 0;

        std::wstring wide_name( module_name.begin( ) , module_name.end( ) );

        if ( Module32FirstW( snapshot , &entry ) ) {
            do {
                if ( _wcsicmp( entry.szModule , wide_name.c_str( ) ) == 0 ) {
                    uintptr_t base = ( uintptr_t ) entry.modBaseAddr;
                    CloseHandle( snapshot );
                    return base;
                }
            } while ( Module32NextW( snapshot , &entry ) );
        }

        CloseHandle( snapshot );
        return 0;
    }
};

std::shared_ptr<c_memory> g_memory;

int main( ) {
    g_memory = std::make_shared<c_memory>( "Discord.exe" );
    if ( !g_memory->valid( ) ) return 1;

    uintptr_t base_addr = g_memory->base_address( ) + 0x0AEADCB8;
    std::vector<uintptr_t> offsets = { 0x0, 0x20 };

    uintptr_t string_addr = g_memory->resolve_pointer( base_addr , offsets );
    std::cout << "resolved string address: 0x" << std::hex << string_addr << "\n";

    std::string value = g_memory->read_string( string_addr , 32 , false , true );
    std::cout << "value: " << value << "\n";
}
