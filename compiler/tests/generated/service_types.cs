

// suppress "Missing XML comment for publicly visible type or member"
#pragma warning disable 1591


#region ReSharper warnings
// ReSharper disable PartialTypeWithSinglePart
// ReSharper disable RedundantNameQualifier
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace
// ReSharper disable UnusedParameter.Local
// ReSharper disable RedundantUsingDirective
#endregion

namespace tests
{
    using System.Collections.Generic;

    [global::Bond.Schema]
    [System.CodeDom.Compiler.GeneratedCode("gbc", "0.11.0.3")]
    public partial class dummy
    {
        [global::Bond.Id(0)]
        public int count { get; set; }

        public dummy()
            : this("tests.dummy", "dummy")
        {}

        protected dummy(string fullName, string name)
        {
            
        }
    }
} // tests
