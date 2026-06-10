#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace drc {

struct RefEntry {
    std::string var;
    int def_line;
    std::vector<int> ref_lines;
    bool is_input;
};

class ScriptAnalyzer {
public:
    ScriptAnalyzer(const std::string& script_path);

    bool valid() const { return m_valid; }
    void normalize();
    void build_ref_table();

    const std::vector<std::string>& lines() const { return m_lines_str; }
    const std::string& normalized_script() const { return m_normalized_script; }
    const std::string& original_script() const { return m_original_script; }
    const std::unordered_map<std::string, RefEntry>& ref_table() const { return m_ref_table; }

    bool has_downstream_refs(const std::string& var, int def_line) const;
    bool is_input_line(int line_num) const;
    std::string get_assigned_var(int line_num) const;
    int num_lines() const { return static_cast<int>(m_lines.size()); }

private:
    struct LineInfo {
        std::string text;
        bool is_input = false;
        std::string assigned_var;
    };

    std::string m_original_script;
    std::vector<LineInfo> m_original_lines;
    std::vector<LineInfo> m_lines;
    std::vector<std::string> m_lines_str;
    std::string m_normalized_script;
    std::unordered_map<std::string, RefEntry> m_ref_table;
    bool m_valid = false;
    bool m_normalized = false;
    int m_temp_counter = 0;

    void split_lines(const std::string& script);
    std::string decompose_chains(const std::string& line);
    std::string strip_local(const std::string& line);
    bool is_comment_or_empty(const std::string& line) const;
    std::string extract_assigned_var(const std::string& line) const;
    std::vector<std::string> extract_referenced_vars(const std::string& expr) const;
    bool is_source_line(const std::string& line) const;
    bool is_target_line(const std::string& line) const;
    bool is_write_line(const std::string& line) const;
    bool is_output_line(const std::string& line) const;
    bool is_input_call(const std::string& line) const;
    void build_normalized();
};

} // namespace drc
