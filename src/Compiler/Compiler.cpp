#include <Ark/Compiler/Compiler.hpp>

namespace Ark
{
    namespace Compiler
    {
        Compiler::Compiler()
        {}

        Compiler::~Compiler()
        {}

        void Compiler::feed(const std::string& code)
        {
            m_parser.feed(code);
            
            if (!m_parser.check())
            {
                Ark::Log::error("[Compiler] Program has errors");
                exit(1);
            }
        }

        void Compiler::compile()
        {
            /*
                Generating headers:
                    - lang name (to be sure we are executing an Ark file)
                        on 4 bytes (ark + padding)
                    - symbols table header
                        + elements
                    - values table header
                        + elements
            */
            m_bytecode.push_back('a');
            m_bytecode.push_back('r');
            m_bytecode.push_back('k');
            m_bytecode.push_back(Instruction::NOP);

            // symbols table
            m_bytecode.push_back(Instruction::SYM_TABLE_START);
                // gather symbols, values, and start to create code segments
                _compile(m_parser.ast());
            // push size
            pushNumber((uint16_t) m_symbols.size());
            // push elements
            for (auto sym : m_symbols)
            {
                // push the string, nul terminated
                for (std::size_t i=0; i < sym.size(); ++i)
                    m_bytecode.push_back(sym[i]);
                m_bytecode.push_back(Instruction::NOP);
            }

            // values table
            m_bytecode.push_back(Instruction::VAL_TABLE_START);
            // push size
            pushNumber((uint16_t) m_values.size());
            // push elements (separated with 0x00)
            for (auto val : m_values)
            {
                if (val.type == ValueType::Number)
                {
                    m_bytecode.push_back(Instruction::NUMBER_TYPE);
                    auto n = std::get<dozerg::HugeNumber>(val.value);
                    std::string t = n.toString(/* base */ 16);
                    for (std::size_t i=0; i < t.size(); ++i)
                        m_bytecode.push_back(t[i]);
                }
                else if (value.type == ValueType::String)
                {
                    m_bytecode.push_back(Instruction::STRING_TYPE);
                    std::string t = std::get<std::string>(val.value);
                    for (std::size_t i=0; i < t.size(); ++i)
                        m_bytecode.push_back(t[i]);
                }

                m_bytecode.push_back(Instruction::NOP);
            }

            // start main code segment
            m_bytecode.push_back(CODE_SEGMENT_START);
            // push number of elements
            if (!m_code_pages.size())
            {
                pushNumber(0x00);
                return;
            }
            pushNumber((uint16_t) m_code_pages[0].size());
            for (auto inst : m_code_pages[0])
            {
                // handle jump to code page (for functions call)
                if (inst.jump_to_page == 0)
                    m_bytecode.push_back(inst.inst);
                else
                    pushNumber(inst.jump_to_page);
            }
        }

        void Compiler::_compile(Node x)
        {
            // register symbols
            if (x.nodeType() == NodeType::Symbol)
            {
                std::string name = x.getStringVal();
                addSymbol(x);
                return;
            }
            // register values
            if (x.nodeType() == NodeType::String || x.nodeType() == NodeType::Number)
            {
                addValue(x);
                return;
            }
            // registering structures
            if (x.list()[0].nodeType() == NodeType::Keyword)
            {
                Keyword n = x.list()[0].keyword();

                if (n == Keyword::If)
                    return _execute((_execute(x.list()[1], env) == falseSym) ? x.list()[3] : x.list()[2], env);
                if (n == Keyword::Set)
                {
                    std::string name = x.list()[1].getStringVal();
                    return env->find(name)[name] = _execute(x.list()[2], env);
                }
                if (n == Keyword::Def)
                    return (*env)[x.list()[1].getStringVal()] = _execute(x.list()[2], env);
                if (n == Keyword::Fun)
                {
                    x.setNodeType(NodeType::Lambda);
                    x.addEnv(env);
                    return x;
                }
                if (n == Keyword::Begin)
                {
                    for (std::size_t i=1; i < x.list().size() - 1; ++i)
                        _execute(x.list()[i], env);
                    return _execute(x.list()[x.list().size() - 1], env);
                }
                if (n == Keyword::While)
                {
                    while (_execute(x.list()[1], env) == trueSym)
                        _execute(x.list()[2], env);
                    return nil;
                }
            }

            Node proc(_execute(x.list()[0], env));
            Nodes exps;
            for (Node::Iterator exp=x.list().begin() + 1; exp != x.list().end(); ++exp)
                exps.push_back(_execute(*exp, env));

            if (proc.nodeType() == NodeType::Lambda)
                return _execute(proc.list()[2], new Environment(proc.list()[1].list(), exps, proc.getEnv()));
            else if (proc.nodeType() == NodeType::Proc)
                return proc.call(exps);
            else
            {
                Ark::Log::error("(Program) not a function");
                exit(1);
            }
        }

        void Compiler::addSymbol(const std::string& sym)
        {
            if (std::find(m_symbols.begin(), m_symbols.end(), sym) == m_symbols.end())
                m_symbols.push_back(sym);
        }

        void Compiler::addValue(Node x)
        {
            Value v(x);
            if (std::find(m_values.begin(), m_values.end(), v) == m_values.end())
                m_values.push_back(v);
        }

        void Compiler::pushNumber(uint16_t n)
        {
            m_bytecode.push_back((n & 0xff00) >> 8);
            m_bytecode.push_back(n & 0x00ff);
        }
    }
}