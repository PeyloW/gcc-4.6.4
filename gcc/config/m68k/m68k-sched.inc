static int m68k_sched_adjust_cost (rtx, rtx, rtx, int);
static int m68k_sched_issue_rate (void);
static int m68k_sched_variable_issue (FILE *, int, rtx, int);
static void m68k_sched_md_init_global (FILE *, int, int);
static void m68k_sched_md_finish_global (FILE *, int);
static void m68k_sched_md_init (FILE *, int, int);
static void m68k_sched_dfa_pre_advance_cycle (void);
static void m68k_sched_dfa_post_advance_cycle (void);
static int m68k_sched_first_cycle_multipass_dfa_lookahead (void);


#undef TARGET_SCHED_ADJUST_COST
#define TARGET_SCHED_ADJUST_COST m68k_sched_adjust_cost

#undef TARGET_SCHED_ISSUE_RATE
#define TARGET_SCHED_ISSUE_RATE m68k_sched_issue_rate

#undef TARGET_SCHED_VARIABLE_ISSUE
#define TARGET_SCHED_VARIABLE_ISSUE m68k_sched_variable_issue

#undef TARGET_SCHED_INIT_GLOBAL
#define TARGET_SCHED_INIT_GLOBAL m68k_sched_md_init_global

#undef TARGET_SCHED_FINISH_GLOBAL
#define TARGET_SCHED_FINISH_GLOBAL m68k_sched_md_finish_global

#undef TARGET_SCHED_INIT
#define TARGET_SCHED_INIT m68k_sched_md_init

#undef TARGET_SCHED_DFA_PRE_ADVANCE_CYCLE
#define TARGET_SCHED_DFA_PRE_ADVANCE_CYCLE m68k_sched_dfa_pre_advance_cycle

#undef TARGET_SCHED_DFA_POST_ADVANCE_CYCLE
#define TARGET_SCHED_DFA_POST_ADVANCE_CYCLE m68k_sched_dfa_post_advance_cycle

#undef TARGET_SCHED_FIRST_CYCLE_MULTIPASS_DFA_LOOKAHEAD
#define TARGET_SCHED_FIRST_CYCLE_MULTIPASS_DFA_LOOKAHEAD	\
  m68k_sched_first_cycle_multipass_dfa_lookahead


/* CPU to schedule the program for.  */
enum attr_cpu m68k_sched_cpu;

/* MAC to schedule the program for.  */
enum attr_mac m68k_sched_mac;

/* Operand type.  */
enum attr_op_type
  {
    /* No operand.  */
    OP_TYPE_NONE,

    /* Integer register.  */
    OP_TYPE_RN,

    /* FP register.  */
    OP_TYPE_FPN,

    /* Implicit mem reference (e.g. stack).  */
    OP_TYPE_MEM1,

    /* Memory without offset or indexing.  EA modes 2, 3 and 4.  */
    OP_TYPE_MEM234,

    /* Memory with offset but without indexing.  EA mode 5.  */
    OP_TYPE_MEM5,

    /* Memory with indexing.  EA mode 6.  */
    OP_TYPE_MEM6,

    /* Memory referenced by absolute address.  EA mode 7.  */
    OP_TYPE_MEM7,

    /* Immediate operand that doesn't require extension word.  */
    OP_TYPE_IMM_Q,

    /* Immediate 16 bit operand.  */
    OP_TYPE_IMM_W,

    /* Immediate 32 bit operand.  */
    OP_TYPE_IMM_L
  };

/* Setup scheduling options.  */
void 
m68k_setup_sched_options ()
{
  if (TUNE_CFV1)
    m68k_sched_cpu = CPU_CFV1;
  else if (TUNE_CFV2)
    m68k_sched_cpu = CPU_CFV2;
  else if (TUNE_CFV3)
    m68k_sched_cpu = CPU_CFV3;
  else if (TUNE_CFV4)
    m68k_sched_cpu = CPU_CFV4;
  else
    {
      m68k_sched_cpu = CPU_UNKNOWN;
      flag_schedule_insns = 0;
      flag_schedule_insns_after_reload = 0;
      flag_modulo_sched = 0;
    }

  if (m68k_sched_cpu != CPU_UNKNOWN)
    {
      if ((m68k_cpu_flags & (FL_CF_EMAC | FL_CF_EMAC_B)) != 0)
        m68k_sched_mac = MAC_CF_EMAC;
      else if ((m68k_cpu_flags & FL_CF_MAC) != 0)
        m68k_sched_mac = MAC_CF_MAC;
      else
        m68k_sched_mac = MAC_NO;
    }
}

/* Return type of memory ADDR_RTX refers to.  */
static enum attr_op_type
sched_address_type (enum machine_mode mode, rtx addr_rtx)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  struct m68k_address address;

  if (symbolic_operand (addr_rtx, VOIDmode))
    return OP_TYPE_MEM7;

  if (!m68k_decompose_address (mode, addr_rtx,
        		       reload_completed, &address))
    {
      gcc_assert (!reload_completed);
      /* Reload will likely fix the address to be in the register.  */
      return OP_TYPE_MEM234;
    }

  if (address.scale != 0)
    return OP_TYPE_MEM6;

  if (address.base != NULL_RTX)
    {
      if (address.offset == NULL_RTX)
        return OP_TYPE_MEM234;

      return OP_TYPE_MEM5;
    }

  gcc_assert (address.offset != NULL_RTX);

  return OP_TYPE_MEM7;
}

/* Return X or Y (depending on OPX_P) operand of INSN.  */
static rtx
sched_get_operand (rtx insn, bool opx_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  int i;

  if (recog_memoized (insn) < 0)
    gcc_unreachable ();

  extract_constrain_insn_cached (insn);

  if (opx_p)
    i = get_attr_opx (insn);
  else
    i = get_attr_opy (insn);

  if (i >= recog_data.n_operands)
    return NULL;

  return recog_data.operand[i];
}

/* Return type of INSN's operand X (if OPX_P) or operand Y (if !OPX_P).
   If ADDRESS_P is true, return type of memory location operand refers to.  */
static enum attr_op_type
sched_attr_op_type (rtx insn, bool opx_p, bool address_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  rtx op;

  op = sched_get_operand (insn, opx_p);

  if (op == NULL)
    {
      gcc_assert (!reload_completed);
      return OP_TYPE_RN;
    }

  if (address_p)
    return sched_address_type (QImode, op);

  if (memory_operand (op, VOIDmode))
    return sched_address_type (GET_MODE (op), XEXP (op, 0));

  if (register_operand (op, VOIDmode))
    {
      if ((!reload_completed && FLOAT_MODE_P (GET_MODE (op)))
          || (reload_completed && FP_REG_P (op)))
        return OP_TYPE_FPN;

      return OP_TYPE_RN;
    }

  if (GET_CODE (op) == CONST_INT)
    {
      int ival;

      ival = INTVAL (op);

      /* Check for quick constants.  */
      switch (get_attr_type (insn))
        {
        case TYPE_ALUQ_L:
          if (IN_RANGE (ival, 1, 8) || IN_RANGE (ival, -8, -1))
            return OP_TYPE_IMM_Q;

          gcc_assert (!reload_completed);
          break;

        case TYPE_MOVEQ_L:
          if (USE_MOVQ (ival))
            return OP_TYPE_IMM_Q;

          gcc_assert (!reload_completed);
          break;

        case TYPE_MOV3Q_L:
          if (valid_mov3q_const (ival))
            return OP_TYPE_IMM_Q;

          gcc_assert (!reload_completed);
          break;

        default:
          break;
        }

      if (IN_RANGE (ival, -0x8000, 0x7fff))
        return OP_TYPE_IMM_W;

      return OP_TYPE_IMM_L;
    }

  if (GET_CODE (op) == CONST_DOUBLE)
    {
      switch (GET_MODE (op))
        {
        case SFmode:
          return OP_TYPE_IMM_W;

        case VOIDmode:
        case DFmode:
          return OP_TYPE_IMM_L;

        default:
          gcc_unreachable ();
        }
    }

  if (GET_CODE (op) == CONST
      || symbolic_operand (op, VOIDmode)
      || LABEL_P (op))
    {
      switch (GET_MODE (op))
        {
        case QImode:
          return OP_TYPE_IMM_Q;

        case HImode:
          return OP_TYPE_IMM_W;

        case SImode:
          return OP_TYPE_IMM_L;

        default:
          if (symbolic_operand (m68k_unwrap_symbol (op, false), VOIDmode))
            /* Just a guess.  */
            return OP_TYPE_IMM_W;

          return OP_TYPE_IMM_L;
        }
    }

  gcc_assert (!reload_completed);

  if (FLOAT_MODE_P (GET_MODE (op)))
    return OP_TYPE_FPN;

  return OP_TYPE_RN;
}

/* Implement opx_type attribute.
   Return type of INSN's operand X.
   If ADDRESS_P is true, return type of memory location operand refers to.  */
enum attr_opx_type
m68k_sched_attr_opx_type (rtx insn, int address_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  switch (sched_attr_op_type (insn, true, address_p != 0))
    {
    case OP_TYPE_RN:
      return OPX_TYPE_RN;

    case OP_TYPE_FPN:
      return OPX_TYPE_FPN;

    case OP_TYPE_MEM1:
      return OPX_TYPE_MEM1;

    case OP_TYPE_MEM234:
      return OPX_TYPE_MEM234;

    case OP_TYPE_MEM5:
      return OPX_TYPE_MEM5;

    case OP_TYPE_MEM6:
      return OPX_TYPE_MEM6;

    case OP_TYPE_MEM7:
      return OPX_TYPE_MEM7;

    case OP_TYPE_IMM_Q:
      return OPX_TYPE_IMM_Q;

    case OP_TYPE_IMM_W:
      return OPX_TYPE_IMM_W;

    case OP_TYPE_IMM_L:
      return OPX_TYPE_IMM_L;

    default:
      gcc_unreachable ();
    }
}

/* Implement opy_type attribute.
   Return type of INSN's operand Y.
   If ADDRESS_P is true, return type of memory location operand refers to.  */
enum attr_opy_type
m68k_sched_attr_opy_type (rtx insn, int address_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  switch (sched_attr_op_type (insn, false, address_p != 0))
    {
    case OP_TYPE_RN:
      return OPY_TYPE_RN;

    case OP_TYPE_FPN:
      return OPY_TYPE_FPN;

    case OP_TYPE_MEM1:
      return OPY_TYPE_MEM1;

    case OP_TYPE_MEM234:
      return OPY_TYPE_MEM234;

    case OP_TYPE_MEM5:
      return OPY_TYPE_MEM5;

    case OP_TYPE_MEM6:
      return OPY_TYPE_MEM6;

    case OP_TYPE_MEM7:
      return OPY_TYPE_MEM7;

    case OP_TYPE_IMM_Q:
      return OPY_TYPE_IMM_Q;

    case OP_TYPE_IMM_W:
      return OPY_TYPE_IMM_W;

    case OP_TYPE_IMM_L:
      return OPY_TYPE_IMM_L;

    default:
      gcc_unreachable ();
    }
}

/* Return size of INSN as int.  */
static int
sched_get_attr_size_int (rtx insn)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  int size;

  switch (get_attr_type (insn))
    {
    case TYPE_IGNORE:
      /* There should be no references to m68k_sched_attr_size for 'ignore'
         instructions.  */
      gcc_unreachable ();
      return 0;

    case TYPE_MUL_L:
      size = 2;
      break;

    default:
      size = 1;
      break;
    }

  switch (get_attr_opx_type (insn))
    {
    case OPX_TYPE_NONE:
    case OPX_TYPE_RN:
    case OPX_TYPE_FPN:
    case OPX_TYPE_MEM1:
    case OPX_TYPE_MEM234:
    case OPY_TYPE_IMM_Q:
      break;

    case OPX_TYPE_MEM5:
    case OPX_TYPE_MEM6:
      /* Here we assume that most absolute references are short.  */
    case OPX_TYPE_MEM7:
    case OPY_TYPE_IMM_W:
      ++size;
      break;

    case OPY_TYPE_IMM_L:
      size += 2;
      break;

    default:
      gcc_unreachable ();
    }

  switch (get_attr_opy_type (insn))
    {
    case OPY_TYPE_NONE:
    case OPY_TYPE_RN:
    case OPY_TYPE_FPN:
    case OPY_TYPE_MEM1:
    case OPY_TYPE_MEM234:
    case OPY_TYPE_IMM_Q:
      break;

    case OPY_TYPE_MEM5:
    case OPY_TYPE_MEM6:
      /* Here we assume that most absolute references are short.  */
    case OPY_TYPE_MEM7:
    case OPY_TYPE_IMM_W:
      ++size;
      break;

    case OPY_TYPE_IMM_L:
      size += 2;
      break;

    default:
      gcc_unreachable ();
    }

  if (size > 3)
    {
      gcc_assert (!reload_completed);

      size = 3;
    }

  return size;
}

/* Return size of INSN as attribute enum value.  */
enum attr_size
m68k_sched_attr_size (rtx insn)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  switch (sched_get_attr_size_int (insn))
    {
    case 1:
      return SIZE_1;

    case 2:
      return SIZE_2;

    case 3:
      return SIZE_3;

    default:
      gcc_unreachable ();
    }
}

/* Return operand X or Y (depending on OPX_P) of INSN,
   if it is a MEM, or NULL overwise.  */
static enum attr_op_type
sched_get_opxy_mem_type (rtx insn, bool opx_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  if (opx_p)
    {
      switch (get_attr_opx_type (insn))
        {
        case OPX_TYPE_NONE:
        case OPX_TYPE_RN:
        case OPX_TYPE_FPN:
        case OPX_TYPE_IMM_Q:
        case OPX_TYPE_IMM_W:
        case OPX_TYPE_IMM_L:
          return OP_TYPE_RN;

        case OPX_TYPE_MEM1:
        case OPX_TYPE_MEM234:
        case OPX_TYPE_MEM5:
        case OPX_TYPE_MEM7:
          return OP_TYPE_MEM1;

        case OPX_TYPE_MEM6:
          return OP_TYPE_MEM6;

        default:
          gcc_unreachable ();
        }
    }
  else
    {
      switch (get_attr_opy_type (insn))
        {
        case OPY_TYPE_NONE:
        case OPY_TYPE_RN:
        case OPY_TYPE_FPN:
        case OPY_TYPE_IMM_Q:
        case OPY_TYPE_IMM_W:
        case OPY_TYPE_IMM_L:
          return OP_TYPE_RN;

        case OPY_TYPE_MEM1:
        case OPY_TYPE_MEM234:
        case OPY_TYPE_MEM5:
        case OPY_TYPE_MEM7:
          return OP_TYPE_MEM1;

        case OPY_TYPE_MEM6:
          return OP_TYPE_MEM6;

        default:
          gcc_unreachable ();
        }
    }
}

/* Implement op_mem attribute.  */
enum attr_op_mem
m68k_sched_attr_op_mem (rtx insn)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  enum attr_op_type opx;
  enum attr_op_type opy;

  opx = sched_get_opxy_mem_type (insn, true);
  opy = sched_get_opxy_mem_type (insn, false);

  if (opy == OP_TYPE_RN && opx == OP_TYPE_RN)
    return OP_MEM_00;

  if (opy == OP_TYPE_RN && opx == OP_TYPE_MEM1)
    {
      switch (get_attr_opx_access (insn))
        {
        case OPX_ACCESS_R:
          return OP_MEM_10;

        case OPX_ACCESS_W:
          return OP_MEM_01;

        case OPX_ACCESS_RW:
          return OP_MEM_11;

        default:
          gcc_unreachable ();
        }
    }

  if (opy == OP_TYPE_RN && opx == OP_TYPE_MEM6)
    {
      switch (get_attr_opx_access (insn))
        {
        case OPX_ACCESS_R:
          return OP_MEM_I0;

        case OPX_ACCESS_W:
          return OP_MEM_0I;

        case OPX_ACCESS_RW:
          return OP_MEM_I1;

        default:
          gcc_unreachable ();
        }
    }

  if (opy == OP_TYPE_MEM1 && opx == OP_TYPE_RN)
    return OP_MEM_10;

  if (opy == OP_TYPE_MEM1 && opx == OP_TYPE_MEM1)
    {
      switch (get_attr_opx_access (insn))
        {
        case OPX_ACCESS_W:
          return OP_MEM_11;

        default:
          gcc_assert (!reload_completed);
          return OP_MEM_11;
        }
    }

  if (opy == OP_TYPE_MEM1 && opx == OP_TYPE_MEM6)
    {
      switch (get_attr_opx_access (insn))
        {
        case OPX_ACCESS_W:
          return OP_MEM_1I;

        default:
          gcc_assert (!reload_completed);
          return OP_MEM_1I;
        }
    }

  if (opy == OP_TYPE_MEM6 && opx == OP_TYPE_RN)
    return OP_MEM_I0;

  if (opy == OP_TYPE_MEM6 && opx == OP_TYPE_MEM1)
    {
      switch (get_attr_opx_access (insn))
        {
        case OPX_ACCESS_W:
          return OP_MEM_I1;

        default:
          gcc_assert (!reload_completed);
          return OP_MEM_I1;
        }
    }

  gcc_assert (opy == OP_TYPE_MEM6 && opx == OP_TYPE_MEM6);
  gcc_assert (!reload_completed);
  return OP_MEM_I1;
}

/* Jump instructions types.  Indexed by INSN_UID.
   The same rtl insn can be expanded into different asm instructions
   depending on the cc0_status.  To properly determine type of jump
   instructions we scan instruction stream and map jumps types to this
   array.  */
static enum attr_type *sched_branch_type;

/* Return the type of the jump insn.  */
enum attr_type
m68k_sched_branch_type (rtx insn)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  enum attr_type type;

  type = sched_branch_type[INSN_UID (insn)];

  gcc_assert (type != 0);

  return type;
}

/* Data for ColdFire V4 index bypass.
   Producer modifies register that is used as index in consumer with
   specified scale.  */
static struct
{
  /* Producer instruction.  */
  rtx pro;

  /* Consumer instruction.  */
  rtx con;

  /* Scale of indexed memory access within consumer.
     Or zero if bypass should not be effective at the moment.  */
  int scale;
} sched_cfv4_bypass_data;

/* An empty state that is used in m68k_sched_adjust_cost.  */
static state_t sched_adjust_cost_state;

/* Implement adjust_cost scheduler hook.
   Return adjusted COST of dependency LINK between DEF_INSN and INSN.  */
static int
m68k_sched_adjust_cost (rtx insn, rtx link ATTRIBUTE_UNUSED, rtx def_insn,
        		int cost)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  int delay;

  if (recog_memoized (def_insn) < 0
      || recog_memoized (insn) < 0)
    return cost;

  if (sched_cfv4_bypass_data.scale == 1)
    /* Handle ColdFire V4 bypass for indexed address with 1x scale.  */
    {
      /* haifa-sched.c: insn_cost () calls bypass_p () just before
         targetm.sched.adjust_cost ().  Hence, we can be relatively sure
         that the data in sched_cfv4_bypass_data is up to date.  */
      gcc_assert (sched_cfv4_bypass_data.pro == def_insn
        	  && sched_cfv4_bypass_data.con == insn);

      if (cost < 3)
        cost = 3;

      sched_cfv4_bypass_data.pro = NULL;
      sched_cfv4_bypass_data.con = NULL;
      sched_cfv4_bypass_data.scale = 0;
    }
  else
    gcc_assert (sched_cfv4_bypass_data.pro == NULL
        	&& sched_cfv4_bypass_data.con == NULL
        	&& sched_cfv4_bypass_data.scale == 0);

  /* Don't try to issue INSN earlier than DFA permits.
     This is especially useful for instructions that write to memory,
     as their true dependence (default) latency is better to be set to 0
     to workaround alias analysis limitations.
     This is, in fact, a machine independent tweak, so, probably,
     it should be moved to haifa-sched.c: insn_cost ().  */
  delay = min_insn_conflict_delay (sched_adjust_cost_state, def_insn, insn);
  if (delay > cost)
    cost = delay;

  return cost;
}

/* Return maximal number of insns that can be scheduled on a single cycle.  */
static int
m68k_sched_issue_rate (void)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  switch (m68k_sched_cpu)
    {
    case CPU_CFV1:
    case CPU_CFV2:
    case CPU_CFV3:
      return 1;

    case CPU_CFV4:
      return 2;

    default:
      gcc_unreachable ();
      return 0;
    }
}

/* Maximal length of instruction for current CPU.
   E.g. it is 3 for any ColdFire core.  */
static int max_insn_size;

/* Data to model instruction buffer of CPU.  */
struct _sched_ib
{
  /* True if instruction buffer model is modeled for current CPU.  */
  bool enabled_p;

  /* Size of the instruction buffer in words.  */
  int size;

  /* Number of filled words in the instruction buffer.  */
  int filled;

  /* Additional information about instruction buffer for CPUs that have
     a buffer of instruction records, rather then a plain buffer
     of instruction words.  */
  struct _sched_ib_records
  {
    /* Size of buffer in records.  */
    int n_insns;

    /* Array to hold data on adjustements made to the size of the buffer.  */
    int *adjust;

    /* Index of the above array.  */
    int adjust_index;
  } records;

  /* An insn that reserves (marks empty) one word in the instruction buffer.  */
  rtx insn;
};

static struct _sched_ib sched_ib;

/* ID of memory unit.  */
static int sched_mem_unit_code;

/* Implementation of the targetm.sched.variable_issue () hook.
   It is called after INSN was issued.  It returns the number of insns
   that can possibly get scheduled on the current cycle.
   It is used here to determine the effect of INSN on the instruction
   buffer.  */
static int
m68k_sched_variable_issue (FILE *sched_dump ATTRIBUTE_UNUSED,
        		   int sched_verbose ATTRIBUTE_UNUSED,
        		   rtx insn, int can_issue_more)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  int insn_size;

  if (recog_memoized (insn) >= 0 && get_attr_type (insn) != TYPE_IGNORE)
    {
      switch (m68k_sched_cpu)
        {
        case CPU_CFV1:
        case CPU_CFV2:
          insn_size = sched_get_attr_size_int (insn);
          break;

        case CPU_CFV3:
          insn_size = sched_get_attr_size_int (insn);
          
          /* ColdFire V3 and V4 cores have instruction buffers that can
             accumulate up to 8 instructions regardless of instructions'
             sizes.  So we should take care not to "prefetch" 24 one-word
             or 12 two-words instructions.
             To model this behavior we temporarily decrease size of the
             buffer by (max_insn_size - insn_size) for next 7 instructions.  */
          {
            int adjust;

            adjust = max_insn_size - insn_size;
            sched_ib.size -= adjust;

            if (sched_ib.filled > sched_ib.size)
              sched_ib.filled = sched_ib.size;

            sched_ib.records.adjust[sched_ib.records.adjust_index] = adjust;
          }

          ++sched_ib.records.adjust_index;
          if (sched_ib.records.adjust_index == sched_ib.records.n_insns)
            sched_ib.records.adjust_index = 0;

          /* Undo adjustement we did 7 instructions ago.  */
          sched_ib.size
            += sched_ib.records.adjust[sched_ib.records.adjust_index];

          break;

        case CPU_CFV4:
          gcc_assert (!sched_ib.enabled_p);
          insn_size = 0;
          break;

        default:
          gcc_unreachable ();
        }

      gcc_assert (insn_size <= sched_ib.filled);
      --can_issue_more;
    }
  else if (GET_CODE (PATTERN (insn)) == ASM_INPUT
           || asm_noperands (PATTERN (insn)) >= 0)
    insn_size = sched_ib.filled;
  else
    insn_size = 0;

  sched_ib.filled -= insn_size;

  return can_issue_more;
}

/* Return how many instructions should scheduler lookahead to choose the
   best one.  */
static int
m68k_sched_first_cycle_multipass_dfa_lookahead (void)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  return m68k_sched_issue_rate () - 1;
}

/* Implementation of targetm.sched.init_global () hook.
   It is invoked once per scheduling pass and is used here
   to initialize scheduler constants.  */
static void
m68k_sched_md_init_global (FILE *sched_dump ATTRIBUTE_UNUSED,
        		   int sched_verbose ATTRIBUTE_UNUSED,
        		   int n_insns ATTRIBUTE_UNUSED)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  /* Init branch types.  */
  {
    rtx insn;

    sched_branch_type = XCNEWVEC (enum attr_type, get_max_uid () + 1);

    for (insn = get_insns (); insn != NULL_RTX; insn = NEXT_INSN (insn))
      {
        if (JUMP_P (insn))
          /* !!! FIXME: Implement real scan here.  */
          sched_branch_type[INSN_UID (insn)] = TYPE_BCC;
      }
  }

#ifdef ENABLE_CHECKING
  /* Check that all instructions have DFA reservations and
     that all instructions can be issued from a clean state.  */
  {
    rtx insn;
    state_t state;

    state = alloca (state_size ());

    for (insn = get_insns (); insn != NULL_RTX; insn = NEXT_INSN (insn))
      {
 	if (INSN_P (insn) && recog_memoized (insn) >= 0)
          {
 	    gcc_assert (insn_has_dfa_reservation_p (insn));

 	    state_reset (state);
 	    if (state_transition (state, insn) >= 0)
 	      gcc_unreachable ();
 	  }
      }
  }
#endif

  /* Setup target cpu.  */

  /* ColdFire V4 has a set of features to keep its instruction buffer full
     (e.g., a separate memory bus for instructions) and, hence, we do not model
     buffer for this CPU.  */
  sched_ib.enabled_p = (m68k_sched_cpu != CPU_CFV4);

  switch (m68k_sched_cpu)
    {
    case CPU_CFV4:
      sched_ib.filled = 0;

      /* FALLTHRU */

    case CPU_CFV1:
    case CPU_CFV2:
      max_insn_size = 3;
      sched_ib.records.n_insns = 0;
      sched_ib.records.adjust = NULL;
      break;

    case CPU_CFV3:
      max_insn_size = 3;
      sched_ib.records.n_insns = 8;
      sched_ib.records.adjust = XNEWVEC (int, sched_ib.records.n_insns);
      break;

    default:
      gcc_unreachable ();
    }

  sched_mem_unit_code = get_cpu_unit_code ("cf_mem1");

  sched_adjust_cost_state = xmalloc (state_size ());
  state_reset (sched_adjust_cost_state);

  start_sequence ();
  emit_insn (gen_ib ());
  sched_ib.insn = get_insns ();
  end_sequence ();
}

/* Scheduling pass is now finished.  Free/reset static variables.  */
static void
m68k_sched_md_finish_global (FILE *dump ATTRIBUTE_UNUSED,
        		     int verbose ATTRIBUTE_UNUSED)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  sched_ib.insn = NULL;

  free (sched_adjust_cost_state);
  sched_adjust_cost_state = NULL;

  sched_mem_unit_code = 0;

  free (sched_ib.records.adjust);
  sched_ib.records.adjust = NULL;
  sched_ib.records.n_insns = 0;
  max_insn_size = 0;

  free (sched_branch_type);
  sched_branch_type = NULL;
}

/* Implementation of targetm.sched.init () hook.
   It is invoked each time scheduler starts on the new block (basic block or
   extended basic block).  */
static void
m68k_sched_md_init (FILE *sched_dump ATTRIBUTE_UNUSED,
        	    int sched_verbose ATTRIBUTE_UNUSED,
        	    int n_insns ATTRIBUTE_UNUSED)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  switch (m68k_sched_cpu)
    {
    case CPU_CFV1:
    case CPU_CFV2:
      sched_ib.size = 6;
      break;

    case CPU_CFV3:
      sched_ib.size = sched_ib.records.n_insns * max_insn_size;

      memset (sched_ib.records.adjust, 0,
              sched_ib.records.n_insns * sizeof (*sched_ib.records.adjust));
      sched_ib.records.adjust_index = 0;
      break;

    case CPU_CFV4:
      gcc_assert (!sched_ib.enabled_p);
      sched_ib.size = 0;
      break;

    default:
      gcc_unreachable ();
    }

  if (sched_ib.enabled_p)
    /* haifa-sched.c: schedule_block () calls advance_cycle () just before
       the first cycle.  Workaround that.  */
    sched_ib.filled = -2;
}

/* Implementation of targetm.sched.dfa_pre_advance_cycle () hook.
   It is invoked just before current cycle finishes and is used here
   to track if instruction buffer got its two words this cycle.  */
static void
m68k_sched_dfa_pre_advance_cycle (void)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  if (!sched_ib.enabled_p)
    return;

  if (!cpu_unit_reservation_p (curr_state, sched_mem_unit_code))
    {
      sched_ib.filled += 2;

      if (sched_ib.filled > sched_ib.size)
        sched_ib.filled = sched_ib.size;
    }
}

/* Implementation of targetm.sched.dfa_post_advance_cycle () hook.
   It is invoked just after new cycle begins and is used here
   to setup number of filled words in the instruction buffer so that
   instructions which won't have all their words prefetched would be
   stalled for a cycle.  */
static void
m68k_sched_dfa_post_advance_cycle (void)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  int i;

  if (!sched_ib.enabled_p)
    return;

  /* Setup number of prefetched instruction words in the instruction
     buffer.  */
  i = max_insn_size - sched_ib.filled;

  while (--i >= 0)
    {
      if (state_transition (curr_state, sched_ib.insn) >= 0)
        gcc_unreachable ();
    }
}

/* Return X or Y (depending on OPX_P) operand of INSN,
   if it is an integer register, or NULL overwise.  */
static rtx
sched_get_reg_operand (rtx insn, bool opx_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  rtx op = NULL;

  if (opx_p)
    {
      if (get_attr_opx_type (insn) == OPX_TYPE_RN)
        {
          op = sched_get_operand (insn, true);
          gcc_assert (op != NULL);

          if (!reload_completed && !REG_P (op))
            return NULL;
        }
    }
  else
    {
      if (get_attr_opy_type (insn) == OPY_TYPE_RN)
        {
          op = sched_get_operand (insn, false);
          gcc_assert (op != NULL);

          if (!reload_completed && !REG_P (op))
            return NULL;
        }
    }

  return op;
}

/* Return true, if X or Y (depending on OPX_P) operand of INSN
   is a MEM.  */
static bool
sched_mem_operand_p (rtx insn, bool opx_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  switch (sched_get_opxy_mem_type (insn, opx_p))
    {
    case OP_TYPE_MEM1:
    case OP_TYPE_MEM6:
      return true;

    default:
      return false;
    }
}

/* Return X or Y (depending on OPX_P) operand of INSN,
   if it is a MEM, or NULL overwise.  */
static rtx
sched_get_mem_operand (rtx insn, bool must_read_p, bool must_write_p)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  bool opx_p;
  bool opy_p;

  opx_p = false;
  opy_p = false;

  if (must_read_p)
    {
      opx_p = true;
      opy_p = true;
    }

  if (must_write_p)
    {
      opx_p = true;
      opy_p = false;
    }

  if (opy_p && sched_mem_operand_p (insn, false))
    return sched_get_operand (insn, false);

  if (opx_p && sched_mem_operand_p (insn, true))
    return sched_get_operand (insn, true);

  gcc_unreachable ();
  return NULL;
}

/* Return non-zero if PRO modifies register used as part of
   address in CON.  */
int
m68k_sched_address_bypass_p (rtx pro, rtx con)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  rtx pro_x;
  rtx con_mem_read;

  pro_x = sched_get_reg_operand (pro, true);
  if (pro_x == NULL)
    return 0;

  con_mem_read = sched_get_mem_operand (con, true, false);
  gcc_assert (con_mem_read != NULL);

  if (reg_mentioned_p (pro_x, con_mem_read))
    return 1;

  return 0;
}

/* Helper function for m68k_sched_indexed_address_bypass_p.
   if PRO modifies register used as index in CON,
   return scale of indexed memory access in CON.  Return zero overwise.  */
static int
sched_get_indexed_address_scale (rtx pro, rtx con)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  rtx reg;
  rtx mem;
  struct m68k_address address;

  reg = sched_get_reg_operand (pro, true);
  if (reg == NULL)
    return 0;

  mem = sched_get_mem_operand (con, true, false);
  gcc_assert (mem != NULL && MEM_P (mem));

  if (!m68k_decompose_address (GET_MODE (mem), XEXP (mem, 0), reload_completed,
        		       &address))
    gcc_unreachable ();

  if (REGNO (reg) == REGNO (address.index))
    {
      gcc_assert (address.scale != 0);
      return address.scale;
    }

  return 0;
}

/* Return non-zero if PRO modifies register used
   as index with scale 2 or 4 in CON.  */
int
m68k_sched_indexed_address_bypass_p (rtx pro, rtx con)
{
  DPRINTFA("Debug: %s\n", __FUNCTION__);
  gcc_assert (sched_cfv4_bypass_data.pro == NULL
              && sched_cfv4_bypass_data.con == NULL
              && sched_cfv4_bypass_data.scale == 0);

  switch (sched_get_indexed_address_scale (pro, con))
    {
    case 1:
      /* We can't have a variable latency bypass, so
         remember to adjust the insn cost in adjust_cost hook.  */
      sched_cfv4_bypass_data.pro = pro;
      sched_cfv4_bypass_data.con = con;
      sched_cfv4_bypass_data.scale = 1;
      return 0;

    case 2:
    case 4:
      return 1;

    default:
      return 0;
    }
}